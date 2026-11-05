#include "svc_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

typedef int (*pfn_nssm_svc_list)(int including_non_nssm, char **out_json);
typedef int (*pfn_nssm_svc_control)(const wchar_t *service_name, const wchar_t *action, char **out_json);
typedef void (*pfn_nssm_svc_free)(void *ptr);

static void get_dll_path(wchar_t *path, size_t max_len) {
  GetModuleFileNameW(NULL, path, (DWORD) max_len);
  PathRemoveFileSpecW(path);
  PathAppendW(path, L"nssm_svc.dll");
}

int svc_loader_list(int including_non_nssm, char **out_json) {
  if (! out_json) return ERROR_INVALID_PARAMETER;
  *out_json = NULL;

  wchar_t dll_path[MAX_PATH];
  get_dll_path(dll_path, _countof(dll_path));

  HMODULE hDll = LoadLibraryW(dll_path);
  if (! hDll) {
    DWORD err = GetLastError();
    char err_buf[512];
    sprintf_s(err_buf, sizeof(err_buf),
      "{\"error\":\"Failed to load nssm_svc.dll (code %lu). Ensure nssm_svc.dll is in the same directory as nssm.exe.\"}",
      err);
    *out_json = _strdup(err_buf);
    return (int) err;
  }

  pfn_nssm_svc_list pList = (pfn_nssm_svc_list) GetProcAddress(hDll, "nssm_svc_list");
  pfn_nssm_svc_free pFree = (pfn_nssm_svc_free) GetProcAddress(hDll, "nssm_svc_free");

  int ret = 0;
  if (! pList || ! pFree) {
    *out_json = _strdup("{\"error\":\"Invalid nssm_svc.dll: missing required exports\"}");
    ret = ERROR_PROC_NOT_FOUND;
  } else {
    char *raw_json = NULL;
    ret = pList(including_non_nssm, &raw_json);
    if (raw_json) {
      *out_json = _strdup(raw_json);
      pFree(raw_json);
    } else {
      char err_buf[256];
      sprintf_s(err_buf, sizeof(err_buf), "{\"error\":\"nssm_svc_list failed with code %d\"}", ret);
      *out_json = _strdup(err_buf);
    }
  }

  /* Unload DLL immediately to keep slave in memory only when needed */
  FreeLibrary(hDll);
  return ret;
}

int svc_loader_control(const wchar_t *service_name, const wchar_t *action, char **out_json) {
  if (! service_name || ! action || ! out_json) return ERROR_INVALID_PARAMETER;
  *out_json = NULL;

  wchar_t dll_path[MAX_PATH];
  get_dll_path(dll_path, _countof(dll_path));

  HMODULE hDll = LoadLibraryW(dll_path);
  if (! hDll) {
    DWORD err = GetLastError();
    char err_buf[512];
    sprintf_s(err_buf, sizeof(err_buf),
      "{\"success\":false,\"error\":\"Failed to load nssm_svc.dll (code %lu). Ensure nssm_svc.dll is in the same directory as nssm.exe.\"}",
      err);
    *out_json = _strdup(err_buf);
    return (int) err;
  }

  pfn_nssm_svc_control pControl = (pfn_nssm_svc_control) GetProcAddress(hDll, "nssm_svc_control");
  pfn_nssm_svc_free pFree = (pfn_nssm_svc_free) GetProcAddress(hDll, "nssm_svc_free");

  int ret = 0;
  if (! pControl || ! pFree) {
    *out_json = _strdup("{\"success\":false,\"error\":\"Invalid nssm_svc.dll: missing required exports\"}");
    ret = ERROR_PROC_NOT_FOUND;
  } else {
    char *raw_json = NULL;
    ret = pControl(service_name, action, &raw_json);
    if (raw_json) {
      *out_json = _strdup(raw_json);
      pFree(raw_json);
    } else {
      char err_buf[256];
      sprintf_s(err_buf, sizeof(err_buf), "{\"success\":false,\"error\":\"nssm_svc_control failed with code %d\"}", ret);
      *out_json = _strdup(err_buf);
    }
  }

  /* Unload DLL immediately */
  FreeLibrary(hDll);
  return ret;
}

void svc_loader_free(char *json) {
  if (json) free(json);
}
