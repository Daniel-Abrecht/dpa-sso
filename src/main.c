// Copyright 2023 Daniel Patrick Abrecht
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdbool.h>
#include "mod_auth.h"
#include "apr_strings.h"
#include "apr_escape.h"
#include "ap_provider.h"
#include "http_log.h"
#include "http_core.h"
#include "http_request.h"
#include "util_cookies.h"
#include "util_script.h"
#include <curl/curl.h>

// Note: You can specify bigger values, but the SSO portal will drop old tokens eventually anyway
#define TOKEN_MAX_VALIDITY 1*60*60

static const char* cookie_flags = "HttpOnly;Path=/;SameSite=None;Secure;Version=1";

static inline int cstr_casecmp_z(const char* s1, const char* s2){
  if(s1 == s2) return 0;
  if(!s1) return -1;
  if(!s2) return 1;
  return ap_cstr_casecmp(s1, s2);
}

module AP_MODULE_DECLARE_DATA auth_dpa_sso_module;

enum fake_basic_auth {
  FBA_OFF,
  FBA_USER_ONLY,
  FBA_USER_PASSWORD
};

typedef struct {
  enum fake_basic_auth fakebasicauth;
  apr_uri_t location;
  bool authoritative;
  bool location_set;
  bool authoritative_set;
  bool fakebasicauth_set;
} auth_dpa_sso_config_rec;

static void* create_auth_dpa_sso_dir_config(apr_pool_t* p, char* d){
  auth_dpa_sso_config_rec* conf = apr_pcalloc(p, sizeof(*conf));
  conf->authoritative = true;
  conf->fakebasicauth = false;
  return conf;
}

static void* merge_auth_dpa_sso_dir_config(apr_pool_t* p, void* basev, void* overridesv){
  auth_dpa_sso_config_rec* newconf = apr_pcalloc(p, sizeof(*newconf));
  auth_dpa_sso_config_rec* base = basev;
  auth_dpa_sso_config_rec* overrides = overridesv;

  newconf->location = overrides->location_set ? overrides->location : base->location;
  newconf->location_set = overrides->location_set || base->location_set;
  newconf->authoritative = overrides->authoritative_set ? overrides->authoritative : base->authoritative;
  newconf->authoritative_set = overrides->authoritative_set || base->authoritative_set;
  newconf->fakebasicauth = overrides->fakebasicauth_set ? overrides->fakebasicauth : base->fakebasicauth;
  newconf->fakebasicauth_set = overrides->fakebasicauth_set || base->fakebasicauth_set;

  return newconf;
}

static const char* set_authoritative(cmd_parms* cmd, void *config, int flag){
  auth_dpa_sso_config_rec* conf = config;
  conf->authoritative = flag;
  conf->authoritative_set = true;
  return 0;
}

static const char* set_fake_basic_auth(cmd_parms* cmd, void *config, const char* p){
  enum fake_basic_auth e = FBA_OFF;
  if(!cstr_casecmp_z("off", p)){
    e = FBA_OFF;
  }else if(!cstr_casecmp_z("on", p) || !cstr_casecmp_z("user", p)){
    e = FBA_USER_ONLY;
  }else if(!cstr_casecmp_z("user-password", p)){
    e = FBA_USER_PASSWORD;
  }else{
    return "AuthDPASSOFakeBasicAuth: ERROR: Must be one of: Off, On, user, user-password";
  }
  auth_dpa_sso_config_rec* conf = config;
  conf->fakebasicauth = e;
  conf->fakebasicauth_set = true;
  return 0;
}

static const char* set_location(cmd_parms* cmd, void* config, const char* location){
  auth_dpa_sso_config_rec* conf = config;
  if(apr_uri_parse(cmd->pool, location, &conf->location))
    return apr_psprintf(cmd->pool, "AuthDPASSOLocation must be a valid URL");
  if(cstr_casecmp_z("https", conf->location.scheme))
    return apr_psprintf(cmd->pool, "AuthDPASSOLocation must use HTTPS");
  conf->location_set = true;
  return 0;
}

static const command_rec auth_dpa_sso_cmds[] = {
  AP_INIT_TAKE1("AuthDPASSOLocation"     , set_location       , 0, OR_AUTHCFG, "The location of the SSO Portal"),
  AP_INIT_FLAG ("AuthDPASSOAuthoritative", set_authoritative  , 0, OR_AUTHCFG, "Set to 'Off' to allow access control to be passed along to lower modules if the UserID is not known to this module"),
  AP_INIT_TAKE1("AuthDPASSOFakeBasicAuth", set_fake_basic_auth, 0, OR_AUTHCFG, "Set to 'On' to pass through authentication to the rest of the server as a basic authentication header. The token will be set instead of the password"),
  {0}
};

static char* construct_referer(apr_pool_t* p, const char* uri, request_rec* r){
  unsigned port = ap_get_server_port(r);
  const char *host = ap_get_server_name_for_url(r);
  // This SSO portal requires HTTPS. If the protocol is not https, maybe it's behind
  // a reverse proxy, so we just assume the real address is https and no port.
  if(port == 443 || !r->server->server_scheme || strcmp("https", r->server->server_scheme))
    return apr_pstrcat(p, "https://", host, uri, NULL);
  // But if it is HTTPS, it's probably the real origin
  return apr_psprintf(p, "https://%s:%u%s", host, port, uri);
}

struct check_token_result {
  char* user;
  char* token;
};

enum e_check_token_result {
  CT_INVALID,
  CT_FAILED,
  CT_VALID
};

static enum e_check_token_result check_token(
  request_rec* r,
  apr_uri_t renew_token_url,
  const char* token,
  struct check_token_result* result
){
  enum e_check_token_result res = CT_FAILED;
  result->user = 0;
  result->token = 0;

  request_rec* m = r;
  while(m->prev) m = m->prev;
  while(m->main) m = m->main;

  // TODO: allow overriding the origin / basepath, or maybe allow to take the original URL from some header or something
  const char* referer = apr_pescape_urlencoded(r->pool, construct_referer(r->pool, "", m));
  renew_token_url.query = renew_token_url.query
                        ? apr_pstrcat(r->pool, renew_token_url.query, "&renew-token=", token, "&referer=", referer, NULL)
                        : apr_pstrcat(r->pool, "renew-token=", token, "&referer=", referer, NULL);
  const char *url = apr_uri_unparse(r->pool, &renew_token_url, APR_URI_UNP_REVEALPASSWORD);

  curl_global_init(CURL_GLOBAL_ALL);
  struct curl_slist* headers = 0;
  CURL* ctx = curl_easy_init();
  curl_easy_setopt(ctx, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(ctx, CURLOPT_NOBODY, 1);
  curl_easy_setopt(ctx, CURLOPT_URL, url);
  curl_easy_setopt(ctx, CURLOPT_NOPROGRESS, 1);

  if(curl_easy_perform(ctx))
    goto error;

  long response_code = 0;
  curl_easy_getinfo(ctx, CURLINFO_RESPONSE_CODE, &response_code);
  if(response_code != 200 && response_code != 303){
    if(response_code == 401 || response_code == 403)
      res = CT_INVALID;
    goto error;
  }
  struct curl_header* huser = 0;
  curl_easy_header(ctx, "X-User", 0, CURLH_HEADER, -1, &huser);
  if(!huser || !huser->value || !huser->value[0])
    goto error;
  result->user = apr_pstrdup(r->pool, huser->value);
  struct curl_header* htoken = 0;
  curl_easy_header(ctx, "X-Token", 0, CURLH_HEADER, -1, &htoken);
  if(!htoken || !htoken->value || !htoken->value[0]){
    result->user = 0;
    goto error;
  }
  result->token = apr_pstrdup(r->pool, htoken->value);

  curl_easy_cleanup(ctx);
  return CT_VALID;

error:
  curl_easy_cleanup(ctx);
  return res;
}

static int authenticate_dpa_sso(request_rec* r){
  auth_dpa_sso_config_rec* conf = ap_get_module_config(r->per_dir_config, &auth_dpa_sso_module);

  if(cstr_casecmp_z(ap_auth_type(r), "dpa-sso"))
    return DECLINED;

  apr_table_unset(r->headers_in, "Authorization");

  request_rec* m = r;
  while(m->prev) m = m->prev;
  while(m->main) m = m->main;

  const char* token = 0;
  ap_cookie_read(r, "dpa-sso-token", &token, true);
  if(token){
    struct check_token_result result;
    enum e_check_token_result ret = check_token(r, conf->location, token, &result);
    token = 0;
    if(ret == CT_FAILED){
      apr_table_addn(r->headers_out, "Cache-Control", "no-store");
      apr_table_addn(r->err_headers_out, "Cache-Control", "no-store");
      ap_cookie_remove(r, "dpa-sso-token", cookie_flags, r->headers_out, r->err_headers_out, NULL);
      return HTTP_INTERNAL_SERVER_ERROR;
    }
    if(ret == CT_VALID){
      token = result.token; // Note: the token may have changed
      r->user = result.user;
    }
  }

  if(!token){
    // TODO: allow overriding the origin
    const char* referer = apr_pescape_urlencoded(r->pool, construct_referer(r->pool, m->unparsed_uri, m));
    apr_uri_t renew_token_url = conf->location;
    renew_token_url.query = renew_token_url.query
                          ? apr_pstrcat(r->pool, renew_token_url.query, "&renew-token&referer=", referer, NULL)
                          : apr_pstrcat(r->pool, "renew-token&referer=", referer, NULL);
    apr_table_addn(r->headers_out, "Cache-Control", "no-store");
    apr_table_addn(r->err_headers_out, "Cache-Control", "no-store");
    ap_cookie_remove(r, "dpa-sso-token", cookie_flags, r->headers_out, r->err_headers_out, NULL);
    apr_table_set(r->headers_out, "Location", apr_uri_unparse(r->pool, &renew_token_url, APR_URI_UNP_REVEALPASSWORD));
    return HTTP_SEE_OTHER;
  }else{
    ap_cookie_write(r, "dpa-sso-token", token, cookie_flags, TOKEN_MAX_VALIDITY, r->headers_out, r->err_headers_out, NULL);
    if(conf->fakebasicauth){
      char* basic = conf->fakebasicauth != FBA_USER_PASSWORD ? r->user : apr_pstrcat(r->pool, r->user, ":", token, NULL);
      char* base64 = ap_pbase64encode(r->pool, basic);
      apr_table_setn(r->headers_in, "Authorization", apr_pstrcat(r->pool, "Basic ", base64, NULL));
    }
    return OK;
  }
}

static int preauth_set_token(request_rec* r){
  if(strcmp(r->uri, "/.well-known/dpa-sso/"))
    return DECLINED;

  extern apr_status_t apreq_parse_query_string(apr_pool_t *pool, apr_table_t *t, const char *qs);
  apr_table_t* GET;
  ap_args_to_table(r, &GET);

  const char* token = apr_table_get(GET, "token");
  if(!token || !token[0] || !token[1]) return DECLINED;

  const char* location = apr_table_get(GET, "location");
  if(!location || !location[0])
    location = "/";
  if(location[0] != '/')
    return HTTP_BAD_REQUEST; // Location must start with a /. This will ensure it's a path, and not an url pointing to some other origin.

  {
    const char* token = 0;
    ap_cookie_read(r, "dpa-sso-token", &token, true);
    if(token){
      // the token will be cleared before rediricting to the SSO portal, so it mustn't be present here.
      // If it is, this may be an attempt to remove the old token in a DOS attack.
      return HTTP_FORBIDDEN;
    }
  }

  apr_table_addn(r->headers_out, "Cache-Control", "no-store");
  apr_table_addn(r->err_headers_out, "Cache-Control", "no-store");
  ap_cookie_write(r, "dpa-sso-token", token, cookie_flags, TOKEN_MAX_VALIDITY, r->headers_out, r->err_headers_out, NULL);
  apr_table_set(r->headers_out, "Location", location);

  return HTTP_SEE_OTHER;
}

static void register_hooks(apr_pool_t* p){
  ap_hook_check_access(preauth_set_token, 0, 0, APR_HOOK_FIRST, AP_AUTH_INTERNAL_PER_URI); // Needs to run before authn if both applicable
  ap_hook_check_authn(authenticate_dpa_sso, 0, 0, APR_HOOK_MIDDLE, AP_AUTH_INTERNAL_PER_CONF);
}

AP_DECLARE_MODULE(auth_dpa_sso) = {
  STANDARD20_MODULE_STUFF,
  create_auth_dpa_sso_dir_config,
  merge_auth_dpa_sso_dir_config,
  0, /* Per-server configuration handler */
  0, /* Merge handler for per-server configurations */
  auth_dpa_sso_cmds,
  register_hooks
};
