#ifndef NSSM_SVC_EXPORTS
#define NSSM_SVC_EXPORTS
#endif
#include "nssm_svc.h"
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

/* Safe dynamic JSON string builder */
typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} json_buf_t;

static bool json_init(json_buf_t *j, size_t initial_cap) {
  if (initial_cap < 256) initial_cap = 256;
  j->buf = (char *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, initial_cap);
  if (! j->buf) return false;
  j->len = 0;
  j->cap = initial_cap;
  j->buf[0] = '\0';
  return true;
}

static bool json_reserve(json_buf_t *j, size_t needed) {
  if (j->len + needed + 1 <= j->cap) return true;
  size_t new_cap = (j->cap * 2 > j->len + needed + 1024) ? j->cap * 2 : j->len + needed + 1024;
  char *new_buf = (char *) HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, j->buf, new_cap);
  if (! new_buf) return false;
  j->buf = new_buf;
  j->cap = new_cap;
  return true;
}

static void json_append_raw(json_buf_t *j, const char *str) {
  if (! str) return;
  size_t slen = strlen(str);
  if (! json_reserve(j, slen)) return;
  memcpy(j->buf + j->len, str, slen);
  j->len += slen;
  j->buf[j->len] = '\0';
}

static void json_append_escaped_w(json_buf_t *j, const wchar_t *wstr) {
  if (! wstr) return;
  /* Convert wchar_t string to UTF-8 first */
  int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (utf8_len <= 1) return;

  char *utf8 = (char *) HeapAlloc(GetProcessHeap(), 0, utf8_len);
  if (! utf8) return;
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8, utf8_len, NULL, NULL);

  /* Escape into json buffer */
  for (int i = 0; utf8[i] != '\0'; i++) {
    unsigned char c = (unsigned char) utf8[i];
    if (c == '"') json_append_raw(j, "\\\"");
    else if (c == '\\') json_append_raw(j, "\\\\");
    else if (c == '\b') json_append_raw(j, "\\b");
    else if (c == '\f') json_append_raw(j, "\\f");
    else if (c == '\n') json_append_raw(j, "\\n");
    else if (c == '\r') json_append_raw(j, "\\r");
    else if (c == '\t') json_append_raw(j, "\\t");
    else if (c < 0x20) {
      char hex[8];
      sprintf_s(hex, sizeof(hex), "\\u%04x", c);
      json_append_raw(j, hex);
    } else {
      char ch[2] = { (char) c, '\0' };
      json_append_raw(j, ch);
    }
  }

  HeapFree(GetProcessHeap(), 0, utf8);
}

static const char *state_to_string(DWORD state) {
  switch (state) {
    case SERVICE_STOPPED: return "STOPPED";
    case SERVICE_START_PENDING: return "START_PENDING";
    case SERVICE_STOP_PENDING: return "STOP_PENDING";
    case SERVICE_RUNNING: return "RUNNING";
    case SERVICE_CONTINUE_PENDING: return "CONTINUE_PENDING";
    case SERVICE_PAUSE_PENDING: return "PAUSE_PENDING";
    case SERVICE_PAUSED: return "PAUSED";
    default: return "UNKNOWN";
  }
}

static const char *start_type_to_string(DWORD start_type) {
  switch (start_type) {
    case SERVICE_AUTO_START: return "AUTO_START";
    case SERVICE_DEMAND_START: return "DEMAND_START";
    case SERVICE_DISABLED: return "DISABLED";
    case SERVICE_BOOT_START: return "BOOT_START";
    case SERVICE_SYSTEM_START: return "SYSTEM_START";
    default: return "UNKNOWN";
  }
}

static DWORD get_service_start_type(SC_HANDLE scm, const wchar_t *service_name) {
  DWORD start_type = 0;
  SC_HANDLE svc = OpenServiceW(scm, service_name, SERVICE_QUERY_CONFIG);
  if (svc) {
    DWORD bytes_needed = 0;
    QueryServiceConfigW(svc, NULL, 0, &bytes_needed);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      QUERY_SERVICE_CONFIGW *config = (QUERY_SERVICE_CONFIGW *) HeapAlloc(GetProcessHeap(), 0, bytes_needed);
      if (config) {
        if (QueryServiceConfigW(svc, config, bytes_needed, &bytes_needed)) {
          start_type = config->dwStartType;
        }
        HeapFree(GetProcessHeap(), 0, config);
      }
    }
    CloseServiceHandle(svc);
  } else {
    /* Fallback to registry */
    wchar_t key_path[512];
    swprintf_s(key_path, 512, L"SYSTEM\\CurrentControlSet\\Services\\%s", service_name);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      DWORD val = 0, size = sizeof(val), type = 0;
      if (RegQueryValueExW(hKey, L"Start", NULL, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
        start_type = val;
      }
      RegCloseKey(hKey);
    }
  }
  return start_type;
}

/* Enumerate services and return JSON */
NSSM_SVC_API int nssm_svc_list(int including_non_nssm, char **out_json) {
  if (! out_json) return ERROR_INVALID_PARAMETER;
  *out_json = NULL;

  SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
  if (! scm) return GetLastError();

  DWORD bytes_needed = 0, count = 0, resume = 0;
  EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &bytes_needed, &count, &resume, NULL);
  if (GetLastError() != ERROR_MORE_DATA) {
    DWORD err = GetLastError();
    CloseServiceHandle(scm);
    return err;
  }

  ENUM_SERVICE_STATUS_PROCESSW *services = (ENUM_SERVICE_STATUS_PROCESSW *) HeapAlloc(GetProcessHeap(), 0, bytes_needed);
  if (! services) {
    CloseServiceHandle(scm);
    return ERROR_NOT_ENOUGH_MEMORY;
  }

  if (! EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, (LPBYTE) services, bytes_needed, &bytes_needed, &count, &resume, NULL)) {
    DWORD err = GetLastError();
    HeapFree(GetProcessHeap(), 0, services);
    CloseServiceHandle(scm);
    return err;
  }

  json_buf_t j;
  if (! json_init(&j, 16384)) {
    HeapFree(GetProcessHeap(), 0, services);
    CloseServiceHandle(scm);
    return ERROR_NOT_ENOUGH_MEMORY;
  }

  json_append_raw(&j, "[\n");
  bool first = true;

  for (DWORD i = 0; i < count; i++) {
    const wchar_t *sname = services[i].lpServiceName;
    const wchar_t *dname = services[i].lpDisplayName;

    /* Check NSSM registry parameters */
    wchar_t reg_path[512];
    swprintf_s(reg_path, 512, L"SYSTEM\\CurrentControlSet\\Services\\%s\\Parameters", sname);
    HKEY hKey = NULL;
    bool is_nssm = false;
    wchar_t app_path[1024] = {0};
    wchar_t app_dir[1024] = {0};
    wchar_t app_params[1024] = {0};

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, reg_path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      DWORD type = 0, bytes = sizeof(app_path);
      if (RegQueryValueExW(hKey, L"Application", NULL, &type, (LPBYTE)app_path, &bytes) == ERROR_SUCCESS) {
        is_nssm = true;
        bytes = sizeof(app_dir);
        RegQueryValueExW(hKey, L"AppDirectory", NULL, &type, (LPBYTE)app_dir, &bytes);
        bytes = sizeof(app_params);
        RegQueryValueExW(hKey, L"AppParameters", NULL, &type, (LPBYTE)app_params, &bytes);
      }
      RegCloseKey(hKey);
    }

    if (! is_nssm && ! including_non_nssm) continue;

    DWORD start_type = get_service_start_type(scm, sname);

    if (! first) json_append_raw(&j, ",\n");
    first = false;

    json_append_raw(&j, "  {\n");

    json_append_raw(&j, "    \"name\": \"");
    json_append_escaped_w(&j, sname);
    json_append_raw(&j, "\",\n");

    json_append_raw(&j, "    \"displayName\": \"");
    json_append_escaped_w(&j, dname ? dname : sname);
    json_append_raw(&j, "\",\n");

    json_append_raw(&j, "    \"status\": \"");
    json_append_raw(&j, state_to_string(services[i].ServiceStatusProcess.dwCurrentState));
    json_append_raw(&j, "\",\n");

    char num_buf[32];
    sprintf_s(num_buf, sizeof(num_buf), "%lu", services[i].ServiceStatusProcess.dwCurrentState);
    json_append_raw(&j, "    \"statusCode\": ");
    json_append_raw(&j, num_buf);
    json_append_raw(&j, ",\n");

    sprintf_s(num_buf, sizeof(num_buf), "%lu", services[i].ServiceStatusProcess.dwProcessId);
    json_append_raw(&j, "    \"pid\": ");
    json_append_raw(&j, num_buf);
    json_append_raw(&j, ",\n");

    json_append_raw(&j, "    \"startType\": \"");
    json_append_raw(&j, start_type_to_string(start_type));
    json_append_raw(&j, "\",\n");

    json_append_raw(&j, "    \"isNssm\": ");
    json_append_raw(&j, is_nssm ? "true" : "false");
    json_append_raw(&j, ",\n");

    json_append_raw(&j, "    \"app\": \"");
    json_append_escaped_w(&j, app_path);
    json_append_raw(&j, "\",\n");

    json_append_raw(&j, "    \"appDir\": \"");
    json_append_escaped_w(&j, app_dir);
    json_append_raw(&j, "\",\n");

    json_append_raw(&j, "    \"appParams\": \"");
    json_append_escaped_w(&j, app_params);
    json_append_raw(&j, "\"\n");

    json_append_raw(&j, "  }");
  }

  json_append_raw(&j, "\n]");

  HeapFree(GetProcessHeap(), 0, services);
  CloseServiceHandle(scm);

  *out_json = j.buf;
  return 0;
}

/* Wait helper for service status transition */
static DWORD wait_for_status(SC_HANDLE svc, DWORD target_status, DWORD timeout_ms) {
  DWORD elapsed = 0;
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytes = 0;

  while (elapsed < timeout_ms) {
    if (! QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytes)) {
      return GetLastError();
    }
    if (ssp.dwCurrentState == target_status) return ERROR_SUCCESS;
    Sleep(100);
    elapsed += 100;
  }
  return ERROR_TIMEOUT;
}

/* Control a service */
NSSM_SVC_API int nssm_svc_control(const wchar_t *service_name, const wchar_t *action, char **out_json) {
  if (! service_name || ! action || ! out_json) return ERROR_INVALID_PARAMETER;
  *out_json = NULL;

  json_buf_t j;
  if (! json_init(&j, 512)) return ERROR_NOT_ENOUGH_MEMORY;

  SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
  if (! scm) {
    DWORD err = GetLastError();
    char buf[256];
    sprintf_s(buf, sizeof(buf), "{\"success\":false,\"error\":\"OpenSCManager failed with error %lu\"}", err);
    json_append_raw(&j, buf);
    *out_json = j.buf;
    return err;
  }

  DWORD access = SERVICE_QUERY_STATUS;
  if (_wcsicmp(action, L"start") == 0) access |= SERVICE_START;
  else if (_wcsicmp(action, L"stop") == 0) access |= SERVICE_STOP;
  else if (_wcsicmp(action, L"restart") == 0) access |= (SERVICE_STOP | SERVICE_START);
  else if (_wcsicmp(action, L"pause") == 0 || _wcsicmp(action, L"continue") == 0) access |= SERVICE_PAUSE_CONTINUE;

  SC_HANDLE svc = OpenServiceW(scm, service_name, access);
  if (! svc) {
    DWORD err = GetLastError();
    CloseServiceHandle(scm);
    char buf[256];
    sprintf_s(buf, sizeof(buf), "{\"success\":false,\"error\":\"OpenService failed: access denied or service not found (code %lu)\"}", err);
    json_append_raw(&j, buf);
    *out_json = j.buf;
    return err;
  }

  DWORD err = ERROR_SUCCESS;
  SERVICE_STATUS sstatus;

  if (_wcsicmp(action, L"start") == 0) {
    if (! StartServiceW(svc, 0, NULL)) {
      err = GetLastError();
      if (err == ERROR_SERVICE_ALREADY_RUNNING) err = ERROR_SUCCESS;
    }
    if (err == ERROR_SUCCESS) wait_for_status(svc, SERVICE_RUNNING, 3000);
  } else if (_wcsicmp(action, L"stop") == 0) {
    if (! ControlService(svc, SERVICE_CONTROL_STOP, &sstatus)) {
      err = GetLastError();
      if (err == ERROR_SERVICE_NOT_ACTIVE) err = ERROR_SUCCESS;
    }
    if (err == ERROR_SUCCESS) wait_for_status(svc, SERVICE_STOPPED, 3000);
  } else if (_wcsicmp(action, L"restart") == 0) {
    ControlService(svc, SERVICE_CONTROL_STOP, &sstatus);
    wait_for_status(svc, SERVICE_STOPPED, 4000);
    if (! StartServiceW(svc, 0, NULL)) {
      err = GetLastError();
    } else {
      wait_for_status(svc, SERVICE_RUNNING, 3000);
    }
  } else if (_wcsicmp(action, L"pause") == 0) {
    if (! ControlService(svc, SERVICE_CONTROL_PAUSE, &sstatus)) err = GetLastError();
  } else if (_wcsicmp(action, L"continue") == 0) {
    if (! ControlService(svc, SERVICE_CONTROL_CONTINUE, &sstatus)) err = GetLastError();
  } else {
    err = ERROR_INVALID_FUNCTION;
  }

  /* Query final status */
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytes = 0;
  const char *final_status = "UNKNOWN";
  if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytes)) {
    final_status = state_to_string(ssp.dwCurrentState);
  }

  CloseServiceHandle(svc);
  CloseServiceHandle(scm);

  if (err == ERROR_SUCCESS) {
    char buf[256];
    sprintf_s(buf, sizeof(buf), "{\"success\":true,\"message\":\"Action completed\",\"status\":\"%s\"}", final_status);
    json_append_raw(&j, buf);
  } else {
    char buf[256];
    sprintf_s(buf, sizeof(buf), "{\"success\":false,\"error\":\"Failed with code %lu\",\"errorCode\":%lu,\"status\":\"%s\"}", err, err, final_status);
    json_append_raw(&j, buf);
  }

  *out_json = j.buf;
  return err;
}

/* Free buffer allocated by nssm_svc.dll */
NSSM_SVC_API void nssm_svc_free(void *ptr) {
  if (ptr) HeapFree(GetProcessHeap(), 0, ptr);
}
