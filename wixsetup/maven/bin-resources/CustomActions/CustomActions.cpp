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

        CopyFile(fromFile, toFile, TRUE);
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

            if (swscanf_s(fdFile.cFileName, L"ecs.db.%d.%d", &major, &minor) != 2)
