// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef _CUSTOM_ACTIONS_UTILS_H_
#define _CUSTOM_ACTIONS_UTILS_H_

#include "Windows.h"
#include <atlstr.h>
#include <Msi.h>

class Error {
public:
    Error(LPCWSTR msg);

    LPCWSTR msg() const;

private:
    CAtlString _msg;
};

LPCWSTR GetProperty(MSIHANDLE hInstall, LPCWSTR name);

CString GenerateGuid();

bool IsPortAvailable(int port);
bool IsPortRangeAvailable(int firstPort, int count);

void InitWinsock();
void FinishWinsock();

bool isStandaloneSystem(const char* host);

CString GetAppDataLocalFolderPath();

#endif // _CUSTOM_ACTIONS_UTILS_H_
