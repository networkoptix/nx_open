// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/* Windows headers. */
#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> /*< You HAVE to include winsock2.h BEFORE windows.h */
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#endif

#include <nx/utils/literal.h>
