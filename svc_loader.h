#ifndef SVC_LOADER_H
#define SVC_LOADER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  Dynamically loads nssm_svc.dll on demand, queries services, and unloads it.
  including_non_nssm: 0 = NSSM only, 1 = all services.
  out_json: newly allocated JSON string. Free with svc_loader_free().
  Returns 0 on success, or error code.
*/
int svc_loader_list(int including_non_nssm, char **out_json);

/*
  Dynamically loads nssm_svc.dll on demand, performs action, and unloads it.
  service_name: target service name.
  action: L"start", L"stop", L"restart", L"pause", L"continue".
  out_json: newly allocated JSON string. Free with svc_loader_free().
  Returns 0 on success, or error code.
*/
int svc_loader_control(const wchar_t *service_name, const wchar_t *action, char **out_json);

/*
  Free memory returned by svc_loader_* functions.
*/
void svc_loader_free(char *json);

#ifdef __cplusplus
}
#endif

#endif /* SVC_LOADER_H */
