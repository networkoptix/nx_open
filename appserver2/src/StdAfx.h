#include <nx/utils/compiler_options.h>

/* Windows headers. */
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> /*< You HAVE to include winsock2.h BEFORE windows.h */
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#endif

#ifdef __cplusplus
    /* This file should be included only main in vms projects. */
    #include <common/common_globals.h>
#endif

/* Boost headers. */
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <nx/utils/literal.h>
#include <nx/utils/deprecation.h>
