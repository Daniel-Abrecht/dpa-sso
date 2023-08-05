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

enum { MAX_PERMISSIONS = 128 };

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

static int splitstr(const int max, char* result[max], const char* cinput, const char*const delimiter){
  if(!cinput || max <= 2)
    return -1;
  char* input = strdup(cinput);
  if(!input)
    return -1;
  int i = 0;
  for(char* res, *tmp=input; (res=strsep(&tmp,delimiter)); ){
    result[i++] = res;
    if(i+1 >= max){
      result[0] = 0;
      free(input);
      return -1;
    }
  }
  result[i] = 0;
  return i;
}

static bool check_permissions(const char*const* required, const char*const* present){
  for(const char*const* r=required; *r; r++){
    for(const char*const* p=present; *p; p++)
      if(!strcmp(*r, *p))
        goto next;
    return false;
  next:;
  }
  return true;
}

__attribute__((visibility("default")))
PAM_EXTERN int pam_sm_authenticate(
  pam_handle_t* pamh, int flags,
  int argc, const char** argv
){
  (void)flags;
  const char* url = 0;
  const char** arge = argv + argc;
  char* permissions[MAX_PERMISSIONS];
  permissions[0] = 0;
  for(int i=-argc; i<0; i++){
    if(i <= -2 && !strcmp("url",arge[i]) && !strncmp("https://",arge[i+1],8)){
      url = arge[++i];
    }else if(!strncmp("url=https://", arge[i], 12)){
      url = arge[i] + 4;
    }else if(i <= -2 && !strcmp("permission",arge[i])){
      splitstr(MAX_PERMISSIONS, permissions, arge[++i], ",");
    }else if(!strncmp("permission=", arge[i], 11)){
      splitstr(MAX_PERMISSIONS, permissions, arge[i] + 11, ",");
    }
  }
#ifdef HAVE_PAM_FAIL_DELAY
  (void) pam_fail_delay(pamh, 5 * 1000000);
#endif
  if(!url){
    pam_syslog(pamh, LOG_ERR, "url was not specified");
    free(permissions[0]);
    return PAM_AUTHINFO_UNAVAIL;
  }
  if(!permissions[0]){
    pam_syslog(pamh, LOG_ERR, "permission list was not specified");
    free(permissions[0]);
    return PAM_AUTHINFO_UNAVAIL;
  }
  const char* user = 0;
  if(pam_get_user(pamh, &user, NULL) != PAM_SUCCESS){
    pam_syslog(pamh, LOG_ERR, "couldn't get username from PAM stack");
    free(permissions[0]);
    return PAM_AUTH_ERR;
  }
  const char* password = 0;
  if(pam_get_authtok(pamh, PAM_AUTHTOK, &password, NULL) != PAM_SUCCESS){
    pam_syslog(pamh, LOG_ERR, "couldn't get password from PAM stack");
    free(permissions[0]);
    return PAM_AUTH_ERR;
  }
  if(!strchr(password, ' ')){
    pam_syslog(pamh, LOG_WARNING, "password isn't a dpa-sso token");
    free(permissions[0]);
    return PAM_AUTH_ERR;
  }

  { // Early plausibility check. Not all permissions may be valid, but if any are missing, we can error out early.
    char* token_parts[MAX_PERMISSIONS];
    int n = splitstr(MAX_PERMISSIONS, token_parts, password, " ");
    if(n < 2){
      pam_syslog(pamh, LOG_WARNING, n>=0 ? "password isn't a dpa-sso token" : "splitstr failed");
      if(n) free(token_parts[0]);
      return PAM_AUTH_ERR;
    }
    token_parts[--n] = 0; // The last part is the actual token
    if(!check_permissions((const char*const*)permissions, (const char*const*)token_parts)){
      pam_syslog(pamh, LOG_WARNING, "The token didn't have some required permissions");
      free(token_parts[0]);
      return PAM_AUTH_ERR;
    }
    free(token_parts[0]);
  }

  struct permission_result* result = check_permission(url, user, password);
  if(!result){
    pam_syslog(pamh, LOG_ERR, "verifying permission token failed");
    free(permissions[0]);
    return PAM_AUTH_ERR;
  }

  if(!check_permissions((const char*const*)permissions, (const char*const*)result->permission)){ // Only here do we have the permissions that were really valid
    pam_syslog(pamh, LOG_ERR, "The token didn't have some required permissions");
    return PAM_AUTH_ERR;
  }
  free_permission_result(result);
  free(permissions[0]);

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
