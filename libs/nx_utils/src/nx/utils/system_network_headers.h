// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// Windows network headers mut be included in a certain order to function properly.
#if defined(_WIN32)
    #if !defined(FD_SETSIZE)
        #define FD_SETSIZE 2048
    #endif

    // winsock2.h must be included before windows.h.
    #include <winsock2.h>

    // windows.h must be included before other network-related libraries.
    #include <windows.h>

    // ws2tcpip.h must be included before iphlpapi.h.
    #include <ws2tcpip.h>

    // Other libraries follow here.
    #include <iphlpapi.h>
#else
    #include <arpa/inet.h>
#endif
