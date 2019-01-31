#if !defined(__STDC_LIMIT_MACROS)
    #define __STDC_LIMIT_MACROS
#endif

#if defined(_WIN32)
    #include <winsock2.h>
    #include <windows.h> /* You HAVE to include winsock2.h BEFORE windows.h */
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#endif
