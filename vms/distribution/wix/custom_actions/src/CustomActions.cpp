// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

struct WcaHelper
{
    WcaHelper(bool useSockets = false):
        m_useSockets(useSockets)
    {
        if (useSockets)
            InitWinsock();
    }

    bool open(MSIHANDLE hInstall, PCSTR actionName)
    {
        HRESULT hr = WcaInitialize(hInstall, actionName);
        const bool success = !FAILED(hr);
        if (success)
            WcaLog(LOGMSG_STANDARD, "Initialized.");
        else
            WcaLogError(hr, "Failed to initialize");

        return success;
    }

    UINT success()
    {
        if (m_useSockets)
            FinishWinsock();

        return WcaFinalize(ERROR_SUCCESS);
    }

    UINT failure()
    {
        if (m_useSockets)
            FinishWinsock();

        return WcaFinalize(ERROR_INSTALL_FAILURE);
    }


private:
    const bool m_useSockets;
};

UINT __stdcall DetectIfSystemIsStandalone(MSIHANDLE hInstall)
{
    WcaHelper wca(/*useSockets*/ true);
    if (!wca.open(hInstall, "DetectIfSystemIsStandalone"))
        return wca.failure();

    CAtlString appserverHost = GetProperty(hInstall, L"SERVER_APPSERVER_HOST");
    CAtlStringA lappserverHost = CAtlStringA(appserverHost);

    MsiSetProperty(hInstall, _T("STANDALONE_SYSTEM"),
        isStandaloneSystem(lappserverHost) ? _T("yes") : _T("no"));

    return wca.success();
}

UINT __stdcall GenerateSystemName(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "GenerateSystemName"))
        return wca.failure();

    // Enterprise Controller Path
    CAtlString registryPath = GetProperty(hInstall, L"OLDEC_REGISTRY_PATH");

    CRegKey RegKey;
    if (RegKey.Open(HKEY_LOCAL_MACHINE, registryPath, KEY_READ | KEY_WOW64_64KEY) != ERROR_SUCCESS)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        return wca.failure();
    }

    TCHAR ecsGuid[1024];
    ULONG pnChars = 1024;
    RegKey.QueryStringValue(_T("ecsGuid"), ecsGuid, &pnChars);

    if (pnChars > 10)
    {
        ecsGuid[9] = 0;
    }

    CAtlString systemName;
    systemName.Format(_T("System_%s"), ecsGuid + 1);
    MsiSetProperty(hInstall, _T("SYSTEM_NAME"), systemName);

    RegKey.Close();
    return wca.success();
}

UINT __stdcall DeleteOldMediaserverSettings(MSIHANDLE hInstall)
{
    WcaHelper wca(/*useSockets*/ true);
    if (!wca.open(hInstall, "DeleteOldMediaserverSettings"))
        return wca.failure();

    CAtlString registryPath = GetProperty(hInstall, L"CustomActionData");

    CRegKey RegKey;
    if (RegKey.Open(
        HKEY_LOCAL_MACHINE, registryPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        return wca.failure();
    }

    RegKey.DeleteValue(L"apiPort");
    RegKey.DeleteValue(L"rtspPort");

    TCHAR appserverHost[1024];
    ULONG pnChars = 1024;
    RegKey.QueryStringValue(_T("appserverHost"), appserverHost, &pnChars);

    CAtlStringA lappserverHost = CAtlStringA(appserverHost);
    if (isStandaloneSystem(lappserverHost))
    {
        RegKey.DeleteValue(L"appserverHost");
        RegKey.DeleteValue(L"appserverPort");
        RegKey.DeleteValue(L"appserverLogin");
        RegKey.DeleteValue(L"appserverPassword");
    }
    else
    {
        RegKey.SetStringValue(L"pendingSwitchToClusterMode", L"yes");
        RegKey.DeleteValue(L"appserverPassword");
    }

    RegKey.Close();
    return wca.success();
}

/**
  * Create new GUIDs for MediaServer and ECS
  */
UINT __stdcall GenerateGuidsForServers(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "GenerateGuidsForServers"))
        return wca.failure();

    MsiSetProperty(hInstall, L"SERVER_GUID_NEW", GenerateGuid());
    MsiSetProperty(hInstall, L"APPSERVER_GUID_NEW", GenerateGuid());

    return wca.success();
}

/**
  * Check if drive which contains directory specified in SERVER_DIRECTORY installer property
  * have space less than 10GB.
  * Set SERVERDIR_LOWDISKSPACE property if so. Otherwize set SERVERDIR_LOWDISKSPACE property to empty string.
  */

UINT __stdcall CheckPorts(MSIHANDLE hInstall)
{
    WcaHelper wca(/*useSockets*/ true);
    if (!wca.open(hInstall, "CheckPorts"))
        return wca.failure();

    CAtlString portsString = GetProperty(hInstall, L"PORTSTOTEST");
    int curPos = 0;

    bool foundBusyPort = false;
    CAtlString portString = portsString.Tokenize(L" ", curPos);
    while (portString != L"")
    {
        int port = _wtoi(portString);

        if (!IsPortAvailable(port))
        {
            MsiSetProperty(hInstall, L"BUSYPORT", portString);
            foundBusyPort = true;
            break;
        }

        portString = portsString.Tokenize(L" ",curPos);
    }

    if (!foundBusyPort)
        MsiSetProperty(hInstall, L"BUSYPORT", L"");

    return wca.success();
}

UINT __stdcall FindFreePorts(MSIHANDLE hInstall)
{
    WcaHelper wca(/*useSockets*/ true);
    if (!wca.open(hInstall, "FindFreePorts"))
        return wca.failure();

    static const int PORTS_TO_ALLOCATE = 4;
    static const int PORT_BASE = 7001;
    static const int PORT_RANGE_STEP = 10;
    static const int PORT_RANGES_TO_TRY = 100;

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

    return wca.success();
}

UINT __stdcall DeleteDatabaseFile(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "DeleteDatabaseFile"))
        return wca.failure();

    try
    {
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
    }
    catch (const Error& e)
    {
        WcaLog(LOGMSG_STANDARD, "DeleteDatabaseFile(): Error: %S", e.msg());
    }

    return wca.success();
}

UINT __stdcall DeleteRegistryKeys(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "DeleteRegistryKeys"))
        return wca.failure();

    CAtlString registryPath = GetProperty(hInstall, L"CustomActionData");

    CRegKey RegKey;
    if (RegKey.Open(
        HKEY_LOCAL_MACHINE, registryPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        return wca.failure();
    }

    RegKey.DeleteValue(L"sysIdTime");
    RegKey.Close();
    return wca.success();
}

UINT __stdcall DeleteAllRegistryKeys(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "DeleteAllRegistryKeys"))
        return wca.failure();

    CRegKey RegKey;

    CAtlString registryPath = GetProperty(hInstall, L"CustomActionData");
    int lastSlashPos = registryPath.ReverseFind(L'\\');

    CAtlString keyParent = registryPath.Mid(0, lastSlashPos);
    CAtlString keyName = registryPath.Mid(lastSlashPos + 1);

    if (RegKey.Open(
        HKEY_LOCAL_MACHINE, keyParent, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)keyParent);
        return wca.failure();
    }

    RegKey.RecurseDeleteKey(keyName);
    RegKey.Close();
    return wca.success();
}

UINT __stdcall CopyHostedFiles(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "CopyHostedFiles"))
        return wca.failure();

    CAtlString params = GetProperty(hInstall, L"CustomActionData");

    int curPos = 0;
    CAtlString fromFile = params.Tokenize(_T(";"), curPos);
    CAtlString toFile = params.Tokenize(_T(";"), curPos);

    CString localAppDataFolder = GetAppDataLocalFolderPath();
    toFile.Replace(L"#LocalAppDataFolder#", localAppDataFolder);

    if (!PathFileExists(toFile))
    {
        SHFILEOPSTRUCT s = { 0 };
        s.hwnd = 0;
        s.wFunc = FO_COPY;
        s.pFrom = fromFile;
        s.pTo = toFile;
        s.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI;
        SHFileOperation(&s);
    }

    return wca.success();
}

UINT __stdcall SetPreviousServerInstalled(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "SetPreviousServerInstalled"))
        return wca.failure();

    CAtlString upgradeCode;
    INSTALLSTATE state;
    TCHAR productCode[39];

    ZeroMemory(productCode, sizeof(productCode));

    upgradeCode = GetProperty(hInstall, L"UpgradeCode");
    for (int n = 0; ; n++)
    {
        UINT res = MsiEnumRelatedProducts(upgradeCode, 0, n, productCode);
        if (res == ERROR_NO_MORE_ITEMS)
            break;

        if (res != ERROR_SUCCESS)
            return wca.failure();
    }

    state = MsiQueryFeatureState(productCode, L"ServerFeature");
    if (state == INSTALLSTATE_LOCAL)
        MsiSetProperty(hInstall, L"PREVIOUSSERVERINSTALLED", L"1");
    return wca.success();
}

UINT __stdcall CleanAutorunRegistryKeys(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "CleanAutorunRegistryKeys"))
        return wca.failure();

    CAtlString registryPath(L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    CAtlString keyPrefix = GetProperty(hInstall, L"CLIENT_NAME");
    WcaLog(LOGMSG_STANDARD, "Key Prefix is: %S", (LPCWSTR)keyPrefix);

    CRegKey RegKey;

    if (RegKey.Open(
        HKEY_CURRENT_USER, registryPath, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) != ERROR_SUCCESS)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        return wca.failure();
    }

    if (keyPrefix.GetLength() == 0)
    {
        WcaLog(LOGMSG_STANDARD, "Client name is empty");
        return wca.failure();
    }

    DWORD dwSize = MAX_PATH, dwIndex = 0;
    TCHAR tcsStringName[MAX_PATH];

    std::vector<CAtlString> valuesToRemove;

    while (RegEnumValue(
        RegKey.m_hKey, dwIndex, tcsStringName, &dwSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
    {
        CAtlString stringName(tcsStringName);
        WcaLog(LOGMSG_STANDARD, "Found key: %S", tcsStringName);
        if (stringName.Find(keyPrefix) == 0)
        {
            valuesToRemove.push_back(stringName);
            WcaLog(LOGMSG_STANDARD, "Deleting");
        }
        else
        {
            WcaLog(LOGMSG_STANDARD, "Skipping");
        }

        dwIndex++;
        dwSize = MAX_PATH;
    }

    for (const auto& value: valuesToRemove)
        RegKey.DeleteValue(value);
    RegKey.Close();

    return wca.success();
}

UINT __stdcall CleanClientRegistryKeys(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "CleanClientRegistryKeys"))
        return wca.failure();

    CAtlString registryPath = GetProperty(hInstall, L"CustomActionData");

    const int lastSlashPos = registryPath.ReverseFind(L'\\');

    const CAtlString keyParent = registryPath.Mid(0, lastSlashPos);
    const CAtlString keyName = registryPath.Mid(lastSlashPos + 1);

    CRegKey regKey;
    if(regKey.Open(HKEY_CURRENT_USER,
        keyParent,
        KEY_READ | KEY_WRITE | KEY_WOW64_64KEY) == ERROR_SUCCESS)
    {
        regKey.RecurseDeleteKey(keyName);
        regKey.Close();
    }
    else
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key `%S` for `%S` deletion. Skipping operation.",
            (LPCWSTR)keyParent, (LPCWSTR)keyName);
    }
    return wca.success();
}

bool dirExists(const CString& dirName)
{
    return GetFileAttributes(dirName) != INVALID_FILE_ATTRIBUTES;
}

UINT __stdcall DeleteClientConfigFiles(MSIHANDLE hInstall)
{
    WcaHelper wca;
    if (!wca.open(hInstall, "DeleteClientConfigFiles"))
        return wca.failure();

    CString configsFolder = GetProperty(hInstall, L"CustomActionData");

    try
    {
        WcaLog(LOGMSG_STANDARD, "Deleting client configs `%S`.", configsFolder);

        if (!dirExists(configsFolder))
        {
            WcaLog(LOGMSG_STANDARD, "Configs directory doesn't exist `%S`.", configsFolder);
            return wca.success();
        }

        configsFolder += '\0';

        SHFILEOPSTRUCT fileOperationDescription = {0};
        fileOperationDescription.wFunc = FO_DELETE;
        fileOperationDescription.pFrom = configsFolder;
        // Do not show any dialogs.
        fileOperationDescription.fFlags =
            FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;

        if (SHFileOperation(&fileOperationDescription) != 0)
        {
            WcaLog(LOGMSG_STANDARD, "Couldn't delete configs directory `%S`.", configsFolder);
            return wca.failure();
        }
    }
    catch (const Error& e)
    {
        WcaLog(LOGMSG_STANDARD, "Couldn't delete client config directory `%S`. Error: %S",
            configsFolder, e.msg());
        return wca.failure();
    }

    return wca.success();
}
