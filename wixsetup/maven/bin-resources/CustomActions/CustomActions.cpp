// CustomActions.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "Utils.h"

UINT __stdcall CopyMediaServerProfile(MSIHANDLE hInstall) {
    return CopyProfile(hInstall, "CopyMediaServerProfile");
}

UINT __stdcall CopyAppServerProfile(MSIHANDLE hInstall) {
    return CopyProfile(hInstall, "CopyAppServerProfile");
}

UINT __stdcall CopyAppServerNotificationFiles(MSIHANDLE hInstall) {
    return CopyProfile(hInstall, "CopyAppServerNotificationFiles", FALSE);
}

UINT __stdcall FindConfiguredStorages(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    DWORD   dwValue = 0;
    CRegKey RegKey;

    CAtlString registryPath;
    CAtlString arch;

    hr = WcaInitialize(hInstall, "FindConfiguredStorages");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    arch = GetProperty(hInstall, L"ARCHITECTURE");

    registryPath = GetProperty(hInstall, L"MEDIASERVER_REGISTRY_PATH");
    registryPath += L"\\storages";

    int flags = KEY_READ;
    if (arch == "x86") {
        flags |= KEY_WOW64_32KEY;
    } else {
        flags |= KEY_WOW64_64KEY;
    }

    if(RegKey.Open(HKEY_LOCAL_MACHINE, registryPath, flags) != ERROR_SUCCESS) {
        WcaLog(LOGMSG_STANDARD, "Couldn't open registry key: %S", (LPCWSTR)registryPath);
        goto LExit;
    }

    DWORD dwType;
    ULONG nBytes = 4;
    DWORD count = 0;

    RegKey.QueryValue(L"size", &dwType, &count, &nBytes);

    {
        CAtlString countString;
        countString.Format(L"%d", count);
        // TODO: no need for NUMBER_OF_STORAGES. save all storages instead.
        MsiSetProperty(hInstall, L"NUMBER_OF_STORAGES", countString);
    }

    RegKey.Close();

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall DetectIfSystemIsStandalone(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    InitWinsock();

    CRegKey RegKey;

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

    {
        CString fileToDelete = GetProperty(hInstall, L"CustomActionData");
        DeleteFile(fileToDelete);
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall BackupDatabaseFile(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "BackupDatabaseFile");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CAtlString params, fromFile, versionPath, versionName;
        params = GetProperty(hInstall, L"CustomActionData");

        // Extract "from" and "to" files from filesString
        int curPos = 0;
        fromFile = params.Tokenize(_T(";"), curPos);
        versionPath = params.Tokenize(_T(";"), curPos);
        versionName = params.Tokenize(_T(";"), curPos);

        CRegKey key;
        static const int MAX_VERSION_SIZE = 50;
        TCHAR szBuffer[MAX_VERSION_SIZE + 1];
        if (key.Open(HKEY_LOCAL_MACHINE, versionPath, KEY_READ | KEY_WRITE) == ERROR_SUCCESS) {
            ULONG chars;
            CAtlString version;

            if (key.QueryStringValue(versionName, 0, &chars) == ERROR_SUCCESS) {
                chars = min(chars, MAX_VERSION_SIZE);
                key.QueryStringValue(versionName, szBuffer, &chars);
                version = szBuffer;
            }

            key.DeleteValue(versionName);
            key.Close();

            if (!version.IsEmpty())
                CopyFile(fromFile, fromFile + "." + version, FALSE);
        }
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall RestoreDatabaseFile(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "RestoreDatabaseFile");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CAtlString params, toFile, version;
        params = GetProperty(hInstall, L"CustomActionData");

        // Extract "to" file and version from filesString
        int curPos = 0;
        toFile = params.Tokenize(_T(";"), curPos);
        version = params.Tokenize(_T(";"), curPos);

        CopyFile(toFile + "." + version, toFile, FALSE);
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall CopyDatabaseFile(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CopyDatabaseFile");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CAtlString params, fromFile, toFile;
        params = GetProperty(hInstall, L"CustomActionData");

        int curPos = 0;
        fromFile = params.Tokenize(_T(";"), curPos);
        toFile = params.Tokenize(_T(";"), curPos);

        if (!PathFileExists(toFile)) {
            CopyFile(fromFile, toFile, TRUE);
            CopyFile(fromFile + L"-shm", toFile + L"-shm", FALSE);
            CopyFile(fromFile + L"-wal", toFile + L"-wal", FALSE);
        }
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall PopulateRestoreVersions(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "PopulateRestoreVersions");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        MSIHANDLE hTable = NULL; 
        MSIHANDLE hColumns = NULL; 

        WIN32_FIND_DATA fdFile; 
        HANDLE hFind = NULL; 

        wchar_t sPath[2048]; 

        CAtlString sDir = GetProperty(hInstall, L"AppserverDbDir");
        wsprintf(sPath, L"%s\\ecs.db.*", sDir);

        if((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
            goto LExit;
        } 

        int n = 100;
        do { 
            if(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            SYSTEMTIME st;
            wchar_t szLocalDate[255], szLocalTime[255];

            int major, minor;
            if (swscanf_s(fdFile.cFileName, L"ecs.db.%d.%d", &major, &minor) != 2)
                continue;

            wchar_t version[1024];
            wsprintf(version, L"%d.%d", major, minor);

            FILETIME ft = fdFile.ftLastWriteTime;
            FileTimeToLocalFileTime(&ft, &ft);
            FileTimeToSystemTime(&ft, &st);
            GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, szLocalDate, 255);
            GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, szLocalTime, 255);
            
            wchar_t label[1024];
            wsprintf(label, L"%s, Last use: %s %s", version, szLocalDate, szLocalTime);
            hr = WcaAddTempRecord(&hTable, &hColumns, L"ComboBox", NULL, 0, 4, L"VERSION_TO_RESTORE", n--, version, label);
        } 
        while (FindNextFile(hFind, &fdFile));

        FindClose(hFind);

        if (hColumns) 
            MsiCloseHandle(hColumns);

        if (hTable) 
            MsiCloseHandle(hTable);
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall CAQuitExecAndWarn(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    CAtlString params, commandLine, warningMsg;

    hr = WcaInitialize(hInstall, "CAQuitExecAndWarn");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    int warningStatus;
    params = GetProperty(hInstall, L"CustomActionData");

    // Extract commandLine, status and warningMsg
    int curPos = 0;
    commandLine = params.Tokenize(_T(";"), curPos);
    warningStatus = _wtoi(params.Tokenize(_T(";"), curPos));
    warningMsg = params.Tokenize(_T(";"), curPos);

    QuitExecAndWarn((const LPWSTR&)commandLine, warningStatus, (const LPWSTR&)warningMsg);

LExit:

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

