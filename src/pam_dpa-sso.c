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
  char* origin;
  char** permission;
};

void free_permission_result(struct permission_result* p){
  free(p->origin);
  for(char**it=p->permission;*it;it++)
    free(*it);
  free(p->permission);
  free(p);
}

size_t noop_cb(void* ptr, size_t size, size_t nmemb, void* data){
  (void)ptr;
  (void)data;
  return size * nmemb;
}

static struct permission_result* check_permission(const char *url, const char* user, const char* token){
  CURL* curl = curl_easy_init();
  if(!curl)
    return 0;

  struct permission_result* result = malloc(sizeof(struct permission_result));
  if(!result)
    goto error;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, noop_cb);

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
  if(res)
    goto error;

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  if(http_code != 200)
    goto error;

  struct curl_header* horigin = 0;
  curl_easy_header(curl, "X-Origin", 0, CURLH_HEADER, -1, &horigin);
  if(!horigin || !horigin->value || !horigin->value[0])
    goto error;
  char* origin = strdup(horigin->value);
  if(!origin)
    goto error;

  enum { MAX_PERMISSIONS = 256 };
  char* permissions[MAX_PERMISSIONS];
  int n = 0;
  for(int i=0; n<MAX_PERMISSIONS; i++){
    struct curl_header* hpermission = 0;
    curl_easy_header(curl, "X-Permission", i, CURLH_HEADER, -1, &hpermission);
    if(!hpermission)
      break;
    if(!hpermission->value || !hpermission->value[0])
      continue;
    char* perm = strdup(hpermission->value);
    if(!perm)
      goto error2;
    permissions[n++] = perm;
  }
  if(n == 0)
    goto error;
  if(n == MAX_PERMISSIONS)
    goto error2;
  permissions[n] = 0;

  char** p = malloc(sizeof(char*[n+1]));
  if(!p) goto error2;
  memcpy(p, permissions, sizeof(char*[n+1]));

  curl_easy_cleanup(curl);

  result->origin = origin;
  result->permission = p;
  return result;

error2:
  while(n--) free(permissions[n]);
  n = 0;
  free(origin);
error:
  curl_easy_cleanup(curl);
  free(result);
  return 0;
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
  for(int i=-argc; i<0; i++){
    if(i <= -2 && !strcmp("url",arge[i]) && !strncmp("https://",arge[i+1],8)){
      url = arge[++i];
    }else if(!strncmp("url=https://", arge[i], 12)){
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
  if(!strchr(password, ' ')){
    pam_syslog(pamh, LOG_WARNING, "password isn't a dpa-sso token");
    return PAM_AUTH_ERR;
  }

  struct permission_result* result = check_permission(url, user, password);
  if(!result){
    pam_syslog(pamh, LOG_ERR, "verifying permission token failed");
    return PAM_AUTH_ERR;
  }

  char* perms = strdup(permissions);
  if(!perms){
    pam_syslog(pamh, LOG_ERR, "strdup failed");
    free(result);
    return PAM_AUTH_ERR;
  }
  for(char *permission, *tmp=perms; (permission=strsep(&tmp,",")); ){
    for(char**p=result->permission; *p; p++)
      if(!strcmp(permission, *p))
        goto next;
    pam_syslog(pamh, LOG_ERR, "The token didn't have some required permissions");
    free(result);
    return PAM_AUTH_ERR;
  next:;
  }
  free(perms);
  free(result);

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
