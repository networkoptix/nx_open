#define QT_NO_CAST_FROM_ASCII

#include <common/config.h>
#ifdef __cplusplus
#   include <common/common_globals.h>
#endif

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

/* Boost headers. */
#include <boost/foreach.hpp>
#include <boost/array.hpp>

/* QT headers. */
#include <QtWidgets/QAction>

#endif
