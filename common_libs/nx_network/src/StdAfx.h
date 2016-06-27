#include <nx/utils/compiler_options.h>

/* Windows headers. */
#ifdef _WIN32
#   define FD_SETSIZE 2048

#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

/* DXVA headers (should be included before ffmpeg headers). */
#   ifdef _USE_DXVA
#       include <d3d9.h>
#       include <dxva2api.h>
#       include <windows.h>
#       include <windowsx.h>
#       include <ole2.h>
#       include <commctrl.h>
#       include <shlwapi.h>
#       include <Strsafe.h>
#   endif
#else
#    include <arpa/inet.h>
#endif

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
