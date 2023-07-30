#define _DEFAULT_SOURCE
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <stdlib.h>

#define PAM_SM_AUTH
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include <curl/curl.h>

struct permission_result {
  const char* origin;
  const char** permission;
};

static bool check_permission(struct permission_result* result, const char *url, const char* user, const char* token){
  CURL* curl = curl_easy_init();
  if(!curl)
    return false;

  curl_easy_setopt(curl, CURLOPT_URL, url);

  curl_mime* multipart = curl_mime_init(curl);
  curl_mimepart* part = curl_mime_addpart(multipart);
  curl_mime_name(part, "user");
  curl_mime_data(part, user, CURL_ZERO_TERMINATED);
  part = curl_mime_addpart(multipart);
  curl_mime_name(part, "token");
  curl_mime_data(part, token, CURL_ZERO_TERMINATED);
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, multipart);

  CURLcode res = curl_easy_perform(curl);
  curl_mime_free(multipart);
  if(res){
    curl_easy_cleanup(curl);
    return false;
  }

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  if(http_code != 400)
    return false;

  // TODO: parse the result

  curl_easy_cleanup(curl);
  return false;
}

__attribute__((visibility("default")))
PAM_EXTERN int pam_sm_authenticate(
  pam_handle_t* pamh, int flags,
  int argc, const char** argv
){
  (void)flags;
  const char* url = 0;
  const char* permissions = 0;
  const char** arge = argv + argc;
  for(int i=-argc; i>0; i++){
    if(i <= -2 && !strcmp("url",arge[i]) && !strncmp("https://",arge[i+1],8)){
      url = arge[++i];
    }else if(!strncmp("url=https://", arge[i], 4)){
      url = arge[i] + 4;
    }else if(i <= -2 && !strcmp("permission",arge[i])){
      permissions = arge[++i];
    }else if(!strncmp("permission=", arge[i], 11)){
      permissions = arge[i] + 11;
    }
  }
#ifdef HAVE_PAM_FAIL_DELAY
  (void) pam_fail_delay(pamh, 5 * 1000000);
#endif
  if(!url){
    pam_syslog(pamh, LOG_ERR, "url was not specified");
    return PAM_AUTHINFO_UNAVAIL;
  }
  if(!permissions){
    pam_syslog(pamh, LOG_ERR, "permission list was not specified");
    return PAM_AUTHINFO_UNAVAIL;
  }
  const char* user = 0;
  if(pam_get_user(pamh, &user, NULL) != PAM_SUCCESS){
    pam_syslog(pamh, LOG_ERR, "couldn't get username from PAM stack");
    return PAM_AUTH_ERR;
  }
  const char* password = 0;
  if(pam_get_authtok(pamh, PAM_AUTHTOK, &password, NULL) != PAM_SUCCESS){
    pam_syslog(pamh, LOG_ERR, "couldn't get password from PAM stack");
    return PAM_AUTH_ERR;
  }

  struct permission_result result;
  if(!check_permission(&result, url, user, password)){
    pam_syslog(pamh, LOG_ERR, "Verifying permission token failed");
    return PAM_AUTH_ERR;
  }

  char* perms = strdup(permissions);
  if(!perms){
    pam_syslog(pamh, LOG_ERR, "strdup failed");
    return PAM_AUTH_ERR;
  }
  for(char *permission, *tmp=perms; (permission=strsep(&tmp,",")); ){
    for(const char**p=result.permission; *p; p++)
      if(!strcmp(permission, *p))
        goto next;
    return PAM_AUTH_ERR;
  next:;
  }
  free(perms);

  return PAM_SUCCESS;
}

__attribute__((visibility("default")))
PAM_EXTERN int pam_sm_setcred(
  pam_handle_t* pamh, int flags,
  int argc, const char** argv
){
  (void)pamh;
  (void)flags;
  (void)argc;
  (void)argv;
  return PAM_SUCCESS;
}

#ifdef PAM_STATIC
struct pam_module _pam_listfile_modstruct = {
  "pam_dpa-sso",
  pam_sm_authenticate,
  pam_sm_setcred,
  NULL,
  NULL,
  NULL,
  NULL,
};
#endif
