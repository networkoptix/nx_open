/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>
#endif

#ifdef __cplusplus

/* STL headers. */
#include <algorithm>
#include <functional>

/* QT headers. */
#include <QtWidgets/QAction>

#endif

#include <nx/utils/literal.h>
