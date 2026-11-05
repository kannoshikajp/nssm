#ifndef WEB_H
#define WEB_H

#include <tchar.h>

/*
  Starts the NSSM Web UI server.
  Command line arguments: [port] (defaults to 8080)
*/
int nssm_web(int argc, TCHAR **argv);

#endif /* WEB_H */
