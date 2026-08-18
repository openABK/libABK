#pragma once

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifdef NO_WINDOWS
#define closesocket close
#include <sys/socket.h>
#endif

#ifndef SOCKET_ERROR
// gethostname return value
#define SOCKET_ERROR -1
#endif 
