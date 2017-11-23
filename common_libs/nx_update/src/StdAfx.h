#include <nx/utils/compiler_options.h>

/* Windows headers. */
#ifdef _WIN32
#   define FD_SETSIZE 2048

#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>
#else
#    include <arpa/inet.h>
#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
