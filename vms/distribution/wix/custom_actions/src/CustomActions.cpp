// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

//#include "CustomActions.h"

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

// WiX Header Files:
#include <Msi.h>
#include <MsiQuery.h>
#include <wcautil.h>

#include <comutil.h>
#include <shobjidl.h>
#include <Shellapi.h>
#include <shlobj.h>

#include <Aclapi.h>

#include <vector>
#include <algorithm>
#include <iterator>

#include "Utils.h"

UINT __stdcall DetectIfSystemIsStandalone(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    InitWinsock();

    CAtlString appserverHost;
    CAtlStringA lappserverHost;

    hr = WcaInitialize(hInstall, "DetectIfSystemIsStandalone");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    appserverHost = GetProperty(hInstall, L"SERVER_APPSERVER_HOST");
    lappserverHost = CAtlStringA(appserverHost);

    MsiSetProperty(hInstall, _T("STANDALONE_SYSTEM"), isStandaloneSystem(lappserverHost) ? _T("yes") : _T("no"));

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall GenerateSystemName(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    CRegKey RegKey;

    CAtlString registryPath;
    CAtlString systemName;


    hr = WcaInitialize(hInstall, "GenerateSystemName");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    // Enterprise Controller Path
    registryPath = GetProperty(hInstall, L"OLDEC_REGISTRY_PATH");

    if(RegKey.Open(HKEY_LOCAL_MACHINE, registryPath, KEY_READ | KEY_WOW64_64KEY) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        goto LExit;
    }

    TCHAR ecsGuid[1024];
    ULONG pnChars = 1024;
    RegKey.QueryStringValue(_T("ecsGuid"), ecsGuid, &pnChars);

    if (pnChars > 10) {
        ecsGuid[9] = 0;
    }

    systemName.Format(_T("System_%s"), ecsGuid + 1);
    MsiSetProperty(hInstall, _T("SYSTEM_NAME"), systemName);

    RegKey.Close();

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall DeleteOldMediaserverSettings(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    InitWinsock();

    CRegKey RegKey;

    CAtlString registryPath;
    CAtlStringA lappserverHost;

    hr = WcaInitialize(hInstall, "DeleteOldMediaserverSettings");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    registryPath = GetProperty(hInstall, L"CustomActionData");

    if(RegKey.Open(HKEY_LOCAL_MACHINE, registryPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        goto LExit;
    }

    RegKey.DeleteValue(L"apiPort");
    RegKey.DeleteValue(L"rtspPort");

    TCHAR appserverHost[1024];
    ULONG pnChars = 1024;
    RegKey.QueryStringValue(_T("appserverHost"), appserverHost, &pnChars);

    lappserverHost = CAtlStringA(appserverHost);
    if (isStandaloneSystem(lappserverHost)) {
        RegKey.DeleteValue(L"appserverHost");
        RegKey.DeleteValue(L"appserverPort");
        RegKey.DeleteValue(L"appserverLogin");
        RegKey.DeleteValue(L"appserverPassword");
    } else {
        RegKey.SetStringValue(L"pendingSwitchToClusterMode", L"yes");
        RegKey.DeleteValue(L"appserverPassword");
    }

    RegKey.Close();

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

/**
  * Create new GUIDs for MediaServer and ECS
  */
UINT __stdcall GenerateGuidsForServers(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "GenerateGuidsForServers");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");


    MsiSetProperty(hInstall, L"SERVER_GUID_NEW", GenerateGuid());
    MsiSetProperty(hInstall, L"APPSERVER_GUID_NEW", GenerateGuid());

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

/**
  * Check if drive which contains directory specified in SERVER_DIRECTORY installer property
  * have space less than 10GB.
  * Set SERVERDIR_LOWDISKSPACE property if so. Otherwize set SERVERDIR_LOWDISKSPACE property to empty string.
  */

UINT __stdcall CheckPorts(MSIHANDLE hInstall)
{
    CAtlString portsString;
    CAtlString portString;

    InitWinsock();

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CheckIfPortFree");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    portsString = GetProperty(hInstall, L"PORTSTOTEST");
    int curPos= 0;

    bool foundBusyPort = false;
    portString = portsString.Tokenize(L" ",curPos);
    while (portString != L"")
    {
        int port = _wtoi(portString);

        if (!IsPortAvailable(port)) {
            MsiSetProperty(hInstall, L"BUSYPORT", portString);
            foundBusyPort = true;
            break;
        }

        portString = portsString.Tokenize(L" ",curPos);
    }

    if (!foundBusyPort)
        MsiSetProperty(hInstall, L"BUSYPORT", L"");

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall FindFreePorts(MSIHANDLE hInstall)
{
    InitWinsock();

    static const int PORTS_TO_ALLOCATE = 4;

    static const int PORT_BASE = 7001;
    static const int PORT_RANGE_STEP = 10;
    static const int PORT_RANGES_TO_TRY = 100;

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "FindFreePorts");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    int firstPort = PORT_BASE;
    for (int i = 0; i < PORT_RANGES_TO_TRY; i++)
    {
        if (!IsPortRangeAvailable(firstPort, PORTS_TO_ALLOCATE))
            firstPort += PORT_RANGE_STEP;
    }

    // Assume we've found a free range
    for (int i = 1; i <= PORTS_TO_ALLOCATE; i++)
    {
        int port = firstPort - 1 + i;

        CString portPropertyName;
        portPropertyName.Format(L"FREEPORT%d", i);

        CString portPropertyValue;
        portPropertyValue.Format(L"%d", port);

        MsiSetProperty(hInstall, portPropertyName, portPropertyValue);
    }

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall DeleteDatabaseFile(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "DeleteDatabaseFile");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    try {
        CString dbFolder = GetProperty(hInstall, L"CustomActionData");

        CString localAppDataFolder = GetAppDataLocalFolderPath();
        dbFolder.Replace(L"#LocalAppDataFolder#", localAppDataFolder);

        WcaLog(LOGMSG_STANDARD, "Deleting ecs and mserver %S with -shm,-val.", dbFolder);

        DeleteFile(dbFolder + L"\\ecs.sqlite");
        DeleteFile(dbFolder + L"\\ecs.sqlite-shm");
        DeleteFile(dbFolder + L"\\ecs.sqlite-wal");
        DeleteFile(dbFolder + L"\\mserver.sqlite");
        DeleteFile(dbFolder + L"\\mserver.sqlite-shm");
        DeleteFile(dbFolder + L"\\mserver.sqlite-wal");
    } catch (const Error& e) {
        WcaLog(LOGMSG_STANDARD, "DeleteDatabaseFile(): Error: %S", e.msg());
    }

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall DeleteRegistryKeys(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    CRegKey RegKey;

    CAtlString registryPath;

    hr = WcaInitialize(hInstall, "DeleteRegistryKeys");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    registryPath = GetProperty(hInstall, L"CustomActionData");

    if(RegKey.Open(HKEY_LOCAL_MACHINE, registryPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        goto LExit;
    }

    RegKey.DeleteValue(L"sysIdTime");

    RegKey.Close();

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall DeleteAllRegistryKeys(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    CRegKey RegKey;

    CAtlString registryPath, keyParent, keyName;

    hr = WcaInitialize(hInstall, "DeleteAllRegistryKeys");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    registryPath = GetProperty(hInstall, L"CustomActionData");

    int lastSlashPos = registryPath.ReverseFind(L'\\');

    keyParent = registryPath.Mid(0, lastSlashPos);
    keyName = registryPath.Mid(lastSlashPos + 1);

    if(RegKey.Open(HKEY_LOCAL_MACHINE, keyParent, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)keyParent);
        goto LExit;
    }

    RegKey.RecurseDeleteKey(keyName);

    RegKey.Close();

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall CopyHostedFiles(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CopyHostedFiles");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CAtlString params, fromFile, toFile;
        params = GetProperty(hInstall, L"CustomActionData");

        int curPos = 0;
        fromFile = params.Tokenize(_T(";"), curPos);
        toFile = params.Tokenize(_T(";"), curPos);

        CString localAppDataFolder = GetAppDataLocalFolderPath();
        toFile.Replace(L"#LocalAppDataFolder#", localAppDataFolder);

        if (!PathFileExists(toFile)) {
            SHFILEOPSTRUCT s = { 0 };
            s.hwnd = 0;
            s.wFunc = FO_COPY;
            s.pFrom = fromFile;
            s.pTo = toFile;
            s.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI;
            SHFileOperation(&s);
        }
    }

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall SetPreviousServerInstalled(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    CAtlString upgradeCode;
    INSTALLSTATE state;
    TCHAR productCode[39];

    ZeroMemory(productCode, sizeof(productCode));

    hr = WcaInitialize(hInstall, "SetPreviousServerInstalled");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    upgradeCode = GetProperty(hInstall, L"UpgradeCode");
    for (int n = 0; ; n++) {
        UINT res = MsiEnumRelatedProducts(upgradeCode, 0, n, productCode);
        if (res == ERROR_NO_MORE_ITEMS)
            break;

        if (res != ERROR_SUCCESS)
            goto LExit;
    }

    state = MsiQueryFeatureState(productCode, L"ServerFeature");
    if (state == INSTALLSTATE_LOCAL)
        MsiSetProperty(hInstall, L"PREVIOUSSERVERINSTALLED", L"1");

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall CleanAutorunRegistryKeys(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CleanAutorunRegistryKeys");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CAtlString registryPath(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
        CAtlString keyPrefix = GetProperty(hInstall, L"CLIENT_NAME");
        WcaLog(LOGMSG_STANDARD, "Key Prefix is: %S", (LPCWSTR)keyPrefix);

        CRegKey RegKey;

        if (RegKey.Open(HKEY_CURRENT_USER, registryPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS) {
            WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
            goto LExit;
        }

		if (keyPrefix.GetLength() == 0) {
			WcaLog(LOGMSG_STANDARD, "Client name is empty");
			goto LExit;
		}
        DWORD dwSize = MAX_PATH, dwIndex = 0;
        TCHAR tcsStringName[MAX_PATH];

		std::vector<CAtlString> valuesToRemove;

        while (RegEnumValue(RegKey.m_hKey, dwIndex, tcsStringName, &dwSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            CAtlString stringName(tcsStringName);
            WcaLog(LOGMSG_STANDARD, "Found key: %S", tcsStringName);
            if (stringName.Find(keyPrefix) == 0) {
				valuesToRemove.push_back(stringName);
                WcaLog(LOGMSG_STANDARD, "Deleting");
            } else {
                WcaLog(LOGMSG_STANDARD, "Skipping");
            }

            dwIndex++;
			dwSize = MAX_PATH;
        }

		for (const auto& value : valuesToRemove) {
			RegKey.DeleteValue(value);
		}
    }

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall CleanClientRegistryKeys(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CleanClientRegistryKeys");
    if (FAILED(hr))
    {
        WcaLogError(hr, "Failed to initialize");
        return WcaFinalize(ERROR_INSTALL_FAILURE);
    }

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    CAtlString registryPath = GetProperty(hInstall, L"CustomActionData");

    const int lastSlashPos = registryPath.ReverseFind(L'\\');

    const CAtlString keyParent = registryPath.Mid(0, lastSlashPos);
    const CAtlString keyName = registryPath.Mid(lastSlashPos + 1);

    CRegKey regKey;
    if(regKey.Open(HKEY_CURRENT_USER,
        keyParent,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)keyParent);
    }
    else
    {
        regKey.RecurseDeleteKey(keyName);
        regKey.Close();
    }

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
