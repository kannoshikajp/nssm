#ifndef WEB_H
#define WEB_H

#include <tchar.h>

/*
  Starts the NSSM Web UI server in foreground.
  Command line arguments: [port] (defaults to 8080)
*/
int nssm_web(int argc, TCHAR **argv);

/*
  Registers NSSM Web as a Windows service to auto-start on boot.
  If port is <= 0, displays a GUI dialog to enter the port.
  If port is > 0, installs the service immediately with the given port.
*/
int nssm_web_install(int port);

/*
  Removes the NSSM Web Windows service.
*/
int nssm_web_remove();

#endif /* WEB_H */
