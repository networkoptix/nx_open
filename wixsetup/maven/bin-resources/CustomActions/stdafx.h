// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Windows Header Files:
#include <windows.h>

#include <tchar.h>
#include <atlstr.h>
#include <atlpath.h>
#include <strsafe.h>
#include <msiquery.h>
#include <userenv.h>

// WiX Header Files:
#include <wcautil.h>

#include <comutil.h>
#include <objbase.h>
#include <shobjidl.h>
#include <Shellapi.h>
#include <shlobj.h>
#include <msxml2.h>

#include <sys/stat.h>

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iphlpapi.h>
#include <Aclapi.h>

#include <stdexcept>
#include <set>
#include <vector>
#include <algorithm>
#include <iterator>

#include <VersionHelpers.h>
