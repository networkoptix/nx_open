#include <common/config.h>
#ifdef __cplusplus
    #include <common/common_globals.h>
#endif

/* Windows headers. */
#ifdef _WIN32
#   include <winsock2.h>
#   include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

#   if _MSC_VER < 1800
#       define noexcept
#   endif

#endif

/* Boost headers. */
#include <boost/foreach.hpp>
#include <boost/array.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/count_if.hpp>

#include <nx/utils/deprecation.h>
