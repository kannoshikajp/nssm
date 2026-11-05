#ifndef NSSM_SVC_H
#define NSSM_SVC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NSSM_SVC_EXPORTS
#define NSSM_SVC_API __declspec(dllexport)
#else
#define NSSM_SVC_API __declspec(dllimport)
#endif

/*
  Enumerate services and return JSON string.
  including_non_nssm: 0 = only NSSM managed services, 1 = all Windows services.
  out_json: caller must free using nssm_svc_free().
  Returns 0 on success, or Win32 error code.
*/
NSSM_SVC_API int nssm_svc_list(int including_non_nssm, char **out_json);

/*
  Control a service (start, stop, restart, pause, continue).
  service_name: Windows service name.
  action: L"start", L"stop", L"restart", L"pause", L"continue".
  out_json: status/error response in JSON. Caller must free using nssm_svc_free().
  Returns 0 on success, or Win32 error code.
*/
NSSM_SVC_API int nssm_svc_control(const wchar_t *service_name, const wchar_t *action, char **out_json);

/*
  Free memory allocated by nssm_svc.dll.
*/
NSSM_SVC_API void nssm_svc_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* NSSM_SVC_H */
