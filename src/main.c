#include <stdbool.h>
#include "mod_auth.h"
#include "apr_strings.h"
#include "apr_escape.h"
#include "ap_provider.h"
#include "http_core.h"
#include "http_request.h"
#include "util_cookies.h"
#include "util_script.h"

static inline int cstr_casecmp_z(const char* s1, const char* s2){
  if(s1 == s2) return 0;
  if(!s1) return -1;
  if(!s2) return 1;
  return ap_cstr_casecmp(s1, s2);
}

module AP_MODULE_DECLARE_DATA auth_dpa_sso_module;

typedef struct {
  apr_uri_t location;
  bool authoritative;
  bool fakebasicauth;
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

static const char* set_fake_basic_auth(cmd_parms* cmd, void *config, int flag){
  auth_dpa_sso_config_rec* conf = config;
  conf->fakebasicauth = flag;
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
  AP_INIT_FLAG ("AuthDPASSOFakeBasicAuth", set_fake_basic_auth, 0, OR_AUTHCFG, "Set to 'On' to pass through authentication to the rest of the server as a basic authentication header. The token will be set instead of the password"),
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

static int authenticate_dpa_sso(request_rec* r){
  auth_dpa_sso_config_rec* conf = ap_get_module_config(r->per_dir_config, &auth_dpa_sso_module);

  if(cstr_casecmp_z(ap_auth_type(r), "dpa-sso"))
    return DECLINED;

  request_rec* m = r;
  while(m->prev) m = m->prev;
  while(m->main) m = m->main;

  const char* token = 0;
  ap_cookie_read(r, "dpa-sso-token", &token, true);
  if(token){

    return HTTP_FORBIDDEN;
  }

  // TODO: allow overriding the origin
  const char* referer = apr_pescape_urlencoded(r->pool, construct_referer(r->pool, m->unparsed_uri, m));
  apr_uri_t renew_token_url = conf->location;
  renew_token_url.query = renew_token_url.query
                        ? apr_pstrcat(r->pool, renew_token_url.query, "renew-token&referer=", referer, NULL)
                        : apr_pstrcat(r->pool, "renew-token&referer=", referer, NULL);
  apr_table_addn(r->headers_out, "Cache-Control", "no-store");
  apr_table_addn(r->err_headers_out, "Cache-Control", "no-store");
  ap_cookie_remove(r, "dpa-sso-token", 0, r->headers_out, r->err_headers_out, NULL);
  apr_table_set(r->headers_out, "Location", apr_uri_unparse(r->pool, &renew_token_url, APR_URI_UNP_REVEALPASSWORD));
  return HTTP_SEE_OTHER;
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
  ap_cookie_write(r, "dpa-sso-token", token, "HttpOnly;Path=/;SameSite=None;Secure;Version=1", 1*60*60, r->headers_out, r->err_headers_out, NULL);
  apr_table_set(r->headers_out, "Location", location);

  return HTTP_SEE_OTHER;
}

static void register_hooks(apr_pool_t* p){
  ap_hook_check_authn(authenticate_dpa_sso, 0, 0, APR_HOOK_MIDDLE, AP_AUTH_INTERNAL_PER_CONF);
  ap_hook_handler(preauth_set_token, 0, 0, APR_HOOK_FIRST);
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
