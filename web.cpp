#include "nssm.h"
#include "svc_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

static const char HTML_PAGE[] = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>NSSM Service Manager</title>
  <style>
    :root {
      --bg: #0b0f19;
      --card-bg: #111827;
      --border: #1f2937;
      --text: #f9fafb;
      --muted: #9ca3af;
      --accent: #6366f1;
      --accent-hover: #4f46e5;
      --success: #10b981;
      --danger: #ef4444;
      --warning: #f59e0b;
      --info: #3b82f6;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; }
    body { background-color: var(--bg); color: var(--text); padding: 24px; min-height: 100vh; }
    .container { max-width: 1400px; margin: 0 auto; }

    header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; flex-wrap: wrap; gap: 16px; }
    .title-group h1 { font-size: 24px; font-weight: 700; color: #fff; display: flex; align-items: center; gap: 10px; }
    .title-group h1 span { font-size: 13px; background: #3730a3; color: #c7d2fe; padding: 3px 8px; border-radius: 9999px; font-weight: 500; }
    .subtitle { font-size: 13px; color: var(--muted); margin-top: 4px; }

    .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin-bottom: 24px; }
    .stat-card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 18px; }
    .stat-card .label { font-size: 13px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 6px; }
    .stat-card .value { font-size: 28px; font-weight: 700; }
    .stat-card.green .value { color: var(--success); }
    .stat-card.red .value { color: var(--danger); }
    .stat-card.purple .value { color: #a78bfa; }

    .toolbar { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 16px; display: flex; gap: 16px; align-items: center; flex-wrap: wrap; margin-bottom: 20px; }
    .search-box { flex: 1; min-width: 250px; position: relative; }
    .search-box input { width: 100%; background: #1e293b; border: 1px solid #334155; border-radius: 8px; padding: 10px 14px; color: #fff; font-size: 14px; outline: none; }
    .search-box input:focus { border-color: var(--accent); }

    .filters { display: flex; gap: 8px; flex-wrap: wrap; }
    .filter-btn { background: #1e293b; border: 1px solid #334155; color: var(--muted); padding: 8px 14px; border-radius: 8px; font-size: 13px; cursor: pointer; transition: 0.2s; }
    .filter-btn:hover { background: #334155; color: #fff; }
    .filter-btn.active { background: var(--accent); border-color: var(--accent); color: #fff; }

    .toggles { display: flex; align-items: center; gap: 18px; margin-left: auto; }
    .checkbox-label { display: flex; align-items: center; gap: 6px; font-size: 13px; color: var(--muted); cursor: pointer; user-select: none; }
    .checkbox-label input { accent-color: var(--accent); cursor: pointer; }

    .btn { background: var(--accent); border: none; color: #fff; padding: 9px 16px; border-radius: 8px; font-size: 13px; font-weight: 600; cursor: pointer; display: inline-flex; align-items: center; gap: 6px; transition: 0.15s; }
    .btn:hover { background: var(--accent-hover); }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; }

    .table-container { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; overflow-x: auto; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.3); }
    table { width: 100%; border-collapse: collapse; text-align: left; font-size: 13px; }
    th { background: #172033; color: var(--muted); padding: 14px 16px; font-weight: 600; text-transform: uppercase; font-size: 11px; letter-spacing: 0.05em; border-bottom: 1px solid var(--border); }
    td { padding: 14px 16px; border-bottom: 1px solid #1a2234; vertical-align: middle; }
    tr:last-child td { border-bottom: none; }
    tr:hover td { background: rgba(255, 255, 255, 0.02); }

    .badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px; border-radius: 9999px; font-size: 11px; font-weight: 600; letter-spacing: 0.03em; }
    .badge-running { background: rgba(16, 185, 129, 0.15); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }
    .badge-stopped { background: rgba(239, 68, 68, 0.15); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.3); }
    .badge-pending { background: rgba(245, 158, 11, 0.15); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.3); }
    .badge-paused  { background: rgba(59, 130, 246, 0.15); color: #60a5fa; border: 1px solid rgba(59, 130, 246, 0.3); }

    .badge-dot { width: 6px; height: 6px; border-radius: 50%; background: currentColor; }
    .badge-nssm { background: #312e81; color: #a5b4fc; border: 1px solid #4338ca; }
    .badge-win32 { background: #1f2937; color: #9ca3af; border: 1px solid #374151; }

    .svc-name { font-weight: 600; color: #fff; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 13px; }
    .svc-display { color: var(--muted); font-size: 12px; margin-top: 2px; }
    .path-text { color: #94a3b8; font-family: ui-monospace, SFMono-Regular, monospace; font-size: 12px; max-width: 320px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; display: block; }

    .actions { display: flex; gap: 6px; }
    .btn-act { padding: 6px 10px; border-radius: 6px; font-size: 12px; font-weight: 600; border: none; cursor: pointer; transition: 0.15s; }
    .btn-start { background: #065f46; color: #6ee7b7; }
    .btn-start:hover { background: #047857; color: #fff; }
    .btn-stop { background: #7f1d1d; color: #fca5a5; }
    .btn-stop:hover { background: #991b1b; color: #fff; }
    .btn-restart { background: #78350f; color: #fde68a; }
    .btn-restart:hover { background: #92400e; color: #fff; }
    .btn-act:disabled { opacity: 0.4; cursor: not-allowed; }

    .toast { position: fixed; bottom: 24px; right: 24px; background: #1e293b; border: 1px solid #334155; padding: 14px 20px; border-radius: 10px; box-shadow: 0 10px 15px -3px rgba(0,0,0,0.5); font-size: 13px; display: none; z-index: 1000; max-width: 400px; }
    .toast.success { border-color: var(--success); color: #34d399; }
    .toast.error { border-color: var(--danger); color: #f87171; }

    .loading-state { text-align: center; padding: 48px; color: var(--muted); font-size: 14px; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="title-group">
        <h1>NSSM Service Manager <span>Master / Slave</span></h1>
        <div class="subtitle">Master Web Server (WinSock2) &bull; Slave Engine (nssm_svc.dll loaded on-demand)</div>
      </div>
      <div>
        <button class="btn" id="btnRefresh" onclick="loadServices()">&#x21bb; Refresh</button>
      </div>
    </header>

    <div class="stats-grid">
      <div class="stat-card">
        <div class="label">Total Listed</div>
        <div class="value" id="statTotal">-</div>
      </div>
      <div class="stat-card purple">
        <div class="label">NSSM Services</div>
        <div class="value" id="statNssm">-</div>
      </div>
      <div class="stat-card green">
        <div class="label">Running</div>
        <div class="value" id="statRunning">-</div>
      </div>
      <div class="stat-card red">
        <div class="label">Stopped</div>
        <div class="value" id="statStopped">-</div>
      </div>
    </div>

    <div class="toolbar">
      <div class="search-box">
        <input type="text" id="searchInput" placeholder="Search services by name, display name, application..." oninput="renderTable()">
      </div>
      <div class="filters">
        <button class="filter-btn active" id="f-nssm" onclick="setFilter('nssm')">NSSM Only</button>
        <button class="filter-btn" id="f-all" onclick="setFilter('all')">All Services</button>
        <button class="filter-btn" id="f-running" onclick="setFilter('running')">Running</button>
        <button class="filter-btn" id="f-stopped" onclick="setFilter('stopped')">Stopped</button>
      </div>
      <div class="toggles">
        <label class="checkbox-label">
          <input type="checkbox" id="chkIncludeNative" onchange="loadServices()"> Include Windows Services
        </label>
        <label class="checkbox-label">
          <input type="checkbox" id="chkAutoRefresh" checked onchange="toggleAutoRefresh()"> Auto-Refresh (5s)
        </label>
      </div>
    </div>

    <div class="table-container">
      <table>
        <thead>
          <tr>
            <th>Type</th>
            <th>Service Name / Display Name</th>
            <th>Status</th>
            <th>PID</th>
            <th>Startup Type</th>
            <th>Application / Path</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="servicesBody">
          <tr><td colspan="7" class="loading-state">Loading services from slave DLL...</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <div class="toast" id="toast"></div>

  <script>
    let services = [];
    let currentFilter = 'nssm';
    let autoRefreshTimer = null;

    function showToast(msg, isError) {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.className = 'toast ' + (isError ? 'error' : 'success');
      t.style.display = 'block';
      setTimeout(() => { t.style.display = 'none'; }, 4000);
    }

    async function loadServices() {
      const btn = document.getElementById('btnRefresh');
      btn.disabled = true;
      const includeAll = document.getElementById('chkIncludeNative').checked ? 1 : 0;
      try {
        const res = await fetch('/api/services?all=' + includeAll);
        const data = await res.json();
        if (data.error) {
          showToast(data.error, true);
        } else {
          services = data;
          renderTable();
        }
      } catch (e) {
        showToast('Connection failed: ' + e.message, true);
      } finally {
        btn.disabled = false;
      }
    }

    function setFilter(f) {
      currentFilter = f;
      document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
      document.getElementById('f-' + f).classList.add('active');
      renderTable();
    }

    function renderTable() {
      const search = (document.getElementById('searchInput').value || '').toLowerCase();
      const filtered = services.filter(s => {
        if (currentFilter === 'nssm' && !s.isNssm) return false;
        if (currentFilter === 'running' && s.status !== 'RUNNING') return false;
        if (currentFilter === 'stopped' && s.status !== 'STOPPED') return false;
        if (!search) return true;
        return (s.name && s.name.toLowerCase().includes(search)) ||
               (s.displayName && s.displayName.toLowerCase().includes(search)) ||
               (s.app && s.app.toLowerCase().includes(search));
      });

      // Update counters
      document.getElementById('statTotal').textContent = services.length;
      document.getElementById('statNssm').textContent = services.filter(s => s.isNssm).length;
      document.getElementById('statRunning').textContent = services.filter(s => s.status === 'RUNNING').length;
      document.getElementById('statStopped').textContent = services.filter(s => s.status === 'STOPPED').length;

      const tbody = document.getElementById('servicesBody');
      if (filtered.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="loading-state">No services match current filter.</td></tr>';
        return;
      }

      tbody.innerHTML = filtered.map(s => {
        let badgeClass = 'badge-stopped';
        if (s.status === 'RUNNING') badgeClass = 'badge-running';
        else if (s.status.includes('PENDING')) badgeClass = 'badge-pending';
        else if (s.status === 'PAUSED') badgeClass = 'badge-paused';

        const isRunning = s.status === 'RUNNING';
        const canStart = s.status !== 'RUNNING' && !s.status.includes('PENDING');
        const canStop = isRunning || s.status === 'PAUSED';

        return `
          <tr>
            <td>
              <span class="badge ${s.isNssm ? 'badge-nssm' : 'badge-win32'}">
                ${s.isNssm ? 'NSSM' : 'Win32'}
              </span>
            </td>
            <td>
              <div class="svc-name">${escapeHtml(s.name)}</div>
              <div class="svc-display">${escapeHtml(s.displayName)}</div>
            </td>
            <td>
              <span class="badge ${badgeClass}">
                <span class="badge-dot"></span>
                ${escapeHtml(s.status)}
              </span>
            </td>
            <td><code>${s.pid > 0 ? s.pid : '-'}</code></td>
            <td><span style="color:#94a3b8">${escapeHtml(s.startType || 'UNKNOWN')}</span></td>
            <td>
              <span class="path-text" title="${escapeHtml((s.app || '') + ' ' + (s.appParams || ''))}">
                ${s.app ? escapeHtml(s.app) : '<span style="color:#475569">Native Windows Executable</span>'}
              </span>
            </td>
            <td>
              <div class="actions">
                ${canStart ? `<button class="btn-act btn-start" onclick="controlSvc('${escapeJs(s.name)}', 'start', this)">Start</button>` : ''}
                ${canStop ? `<button class="btn-act btn-stop" onclick="controlSvc('${escapeJs(s.name)}', 'stop', this)">Stop</button>` : ''}
                ${canStop ? `<button class="btn-act btn-restart" onclick="controlSvc('${escapeJs(s.name)}', 'restart', this)">Restart</button>` : ''}
              </div>
            </td>
          </tr>
        `;
      }).join('');
    }

    async function controlSvc(name, action, btn) {
      if (btn) btn.disabled = true;
      try {
        const res = await fetch(`/api/service/${encodeURIComponent(name)}/${action}`, { method: 'POST' });
        const data = await res.json();
        if (data.success) {
          showToast(`Service "${name}": ${action} succeeded (${data.status})`, false);
          await loadServices();
        } else {
          showToast(`Error on "${name}": ${data.error || 'Failed'}`, true);
        }
      } catch (e) {
        showToast('Request failed: ' + e.message, true);
      } finally {
        if (btn) btn.disabled = false;
      }
    }

    function toggleAutoRefresh() {
      const enabled = document.getElementById('chkAutoRefresh').checked;
      if (autoRefreshTimer) { clearInterval(autoRefreshTimer); autoRefreshTimer = null; }
      if (enabled) {
        autoRefreshTimer = setInterval(loadServices, 5000);
      }
    }

    function escapeHtml(str) {
      if (!str) return '';
      return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    }

    function escapeJs(str) {
      if (!str) return '';
      return String(str).replace(/'/g, "\\'");
    }

    // Initial load and start timer
    loadServices();
    toggleAutoRefresh();
  </script>
</body>
</html>
)html";

static int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void url_decode(char *dst, const char *src, size_t max_len) {
  size_t d = 0;
  for (size_t s = 0; src[s] && d + 1 < max_len; s++) {
    if (src[s] == '%' && src[s+1] && src[s+2]) {
      int hi = hex_val(src[s+1]);
      int lo = hex_val(src[s+2]);
      if (hi >= 0 && lo >= 0) {
        dst[d++] = (char)((hi << 4) | lo);
        s += 2;
        continue;
      }
    } else if (src[s] == '+') {
      dst[d++] = ' ';
      continue;
    }
    dst[d++] = src[s];
  }
  dst[d] = '\0';
}

static void send_response(SOCKET s, int status_code, const char *status_text, const char *content_type, const char *body, size_t body_len) {
  char header[512];
  int hlen = sprintf_s(header, sizeof(header),
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %zu\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "\r\n",
    status_code, status_text, content_type, body_len);

  send(s, header, hlen, 0);
  if (body && body_len > 0) {
    send(s, body, (int) body_len, 0);
  }
}

static DWORD WINAPI handle_client(LPVOID param) {
  SOCKET client = (SOCKET) param;
  char req_buf[4096];
  int received = recv(client, req_buf, sizeof(req_buf) - 1, 0);
  if (received <= 0) {
    closesocket(client);
    return 0;
  }
  req_buf[received] = '\0';

  char method[16] = {0};
  char uri[1024] = {0};
  if (sscanf_s(req_buf, "%15s %1023s", method, (unsigned)sizeof(method), uri, (unsigned)sizeof(uri)) < 2) {
    send_response(client, 400, "Bad Request", "text/plain", "Bad Request", 11);
    closesocket(client);
    return 0;
  }

  /* Route: GET / or GET /index.html */
  if (_stricmp(method, "GET") == 0 && (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0)) {
    send_response(client, 200, "OK", "text/html; charset=utf-8", HTML_PAGE, sizeof(HTML_PAGE) - 1);
  }
  /* Route: GET /api/services */
  else if (_stricmp(method, "GET") == 0 && strncmp(uri, "/api/services", 13) == 0) {
    int include_all = 0;
    if (strstr(uri, "all=1")) include_all = 1;

    char *json = NULL;
    svc_loader_list(include_all, &json);
    if (json) {
      send_response(client, 200, "OK", "application/json; charset=utf-8", json, strlen(json));
      svc_loader_free(json);
    } else {
      const char *err = "{\"error\":\"Service enumeration failed\"}";
      send_response(client, 500, "Internal Server Error", "application/json; charset=utf-8", err, strlen(err));
    }
  }
  /* Route: POST /api/service/<name>/<action> */
  else if (_stricmp(method, "POST") == 0 && strncmp(uri, "/api/service/", 13) == 0) {
    char *path_part = uri + 13;
    char *slash = strrchr(path_part, '/');
    if (! slash) {
      send_response(client, 400, "Bad Request", "application/json", "{\"error\":\"Invalid endpoint\"}", 27);
    } else {
      *slash = '\0';
      const char *raw_name = path_part;
      const char *action_str = slash + 1;

      char decoded_name[512];
      url_decode(decoded_name, raw_name, sizeof(decoded_name));

      wchar_t w_name[512];
      wchar_t w_action[64];
      MultiByteToWideChar(CP_UTF8, 0, decoded_name, -1, w_name, _countof(w_name));
      MultiByteToWideChar(CP_UTF8, 0, action_str, -1, w_action, _countof(w_action));

      char *json = NULL;
      svc_loader_control(w_name, w_action, &json);
      if (json) {
        send_response(client, 200, "OK", "application/json; charset=utf-8", json, strlen(json));
        svc_loader_free(json);
      } else {
        const char *err = "{\"success\":false,\"error\":\"Operation failed\"}";
        send_response(client, 500, "Internal Server Error", "application/json", err, strlen(err));
      }
    }
  }
  else {
    send_response(client, 404, "Not Found", "text/plain", "Not Found", 9);
  }

  closesocket(client);
  return 0;
}

int nssm_web(int argc, TCHAR **argv) {
  int port = 8080;
  if (argc > 0) {
    int p = _ttoi(argv[0]);
    if (p > 0 && p < 65536) port = p;
  }

  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    _ftprintf(stderr, _T("WSAStartup failed: %lu\n"), GetLastError());
    return 1;
  }

  SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_sock == INVALID_SOCKET) {
    _ftprintf(stderr, _T("Failed to create socket: %d\n"), WSAGetLastError());
    WSACleanup();
    return 2;
  }

  int opt = 1;
  setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

  sockaddr_in addr;
  ZeroMemory(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short) port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_sock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    _ftprintf(stderr, _T("Failed to bind port %d: %d\n"), port, WSAGetLastError());
    closesocket(server_sock);
    WSACleanup();
    return 3;
  }

  if (listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
    _ftprintf(stderr, _T("Failed to listen on socket: %d\n"), WSAGetLastError());
    closesocket(server_sock);
    WSACleanup();
    return 4;
  }

  _tprintf(_T("NSSM Web Service Manager listening on http://127.0.0.1:%d\n"), port);
  _tprintf(_T("Slave engine: nssm_svc.dll (loaded dynamically on-demand)\n"));
  _tprintf(_T("Press Ctrl+C to terminate.\n"));

  while (true) {
    SOCKET client = accept(server_sock, NULL, NULL);
    if (client == INVALID_SOCKET) break;
    HANDLE hThread = CreateThread(NULL, 0, handle_client, (LPVOID) client, 0, NULL);
    if (hThread) CloseHandle(hThread);
    else closesocket(client);
  }

  closesocket(server_sock);
  WSACleanup();
  return 0;
}
