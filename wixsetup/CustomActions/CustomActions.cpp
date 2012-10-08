// CustomActions.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "Utils.h"

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
        MsiSetProperty(hInstall, L"NUMBER_OF_STORAGES", countString);
    }

    RegKey.Close();

LExit:

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
  * Check if directory specified in SERVER_DIRECTORY is writable by LocalSystem User
  * Set SERVER_DIR_CANT_WRITE property if so. Otherwize set it to empty string.
  */
UINT __stdcall CheckServerDirectoryWritable(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CheckServerDirectoryWritable");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    LPCWSTR folderToCheck = GetProperty(hInstall, L"SERVER_DIRECTORY");

    WCHAR volumePathName[20];
    GetVolumePathName(folderToCheck, volumePathName, 20);

    DWORD flags;
    GetVolumeInformation(volumePathName, 0, 0, 0, 0, &flags, 0, 0);

    if (flags & FILE_READ_ONLY_VOLUME)
        MsiSetProperty(hInstall, L"SERVERDIR_CANT_WRITE", L"CANTWRITE");
    else
        MsiSetProperty(hInstall, L"SERVERDIR_CANT_WRITE", L"");

LExit:

    if (folderToCheck)
        delete[] folderToCheck;

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
/**
  * Check if drive which contains directory specified in SERVER_DIRECTORY installer property 
  * have space less than 10GB.
  * Set SERVERDIR_LOWDISKSPACE property if so. Otherwize set SERVERDIR_LOWDISKSPACE property to empty string.
  */
UINT __stdcall CheckServerDriveLowSpace(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "CheckServerDriveLowSpace");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    LPCWSTR folderToCheck = GetProperty(hInstall, L"SERVER_DIRECTORY");
    ULARGE_INTEGER freeBytes;

    WCHAR volumePathName[20];
    GetVolumePathName(folderToCheck, volumePathName, 20);
    GetDiskFreeSpaceEx(volumePathName, &freeBytes, 0, 0);

    WCHAR freeGBString[10];
    int freeGB = freeBytes.HighPart * 4 + (freeBytes.LowPart>> 30);

    _ultow_s(freeGB, freeGBString, 10, 10);

    if (freeGB < 10)
        MsiSetProperty(hInstall, L"SERVERDIR_LOWDISKSPACE", freeGBString);
    else
        MsiSetProperty(hInstall, L"SERVERDIR_LOWDISKSPACE", L"");

LExit:
    
    if (folderToCheck)
        delete[] folderToCheck;

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall SetMoviesFolder(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "SetMoviesFolder");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    WCHAR moviesFolderPath[MAX_PATH];

    if (!SHGetSpecialFolderPath(0, moviesFolderPath, CSIDL_MYVIDEO, TRUE))
    {
        SHGetSpecialFolderPath(0, moviesFolderPath, CSIDL_MYDOCUMENTS, FALSE);
    }

    MsiSetProperty(hInstall, L"MOVIESFOLDER", moviesFolderPath);

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

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

void fixPath(CString& path)
{
    path.Replace(L"/", L"\\");
}

UINT __stdcall FixServerFolder(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "FixServerFolder");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CString serverFolder = GetProperty(hInstall, L"SERVER_DIRECTORY");
        fixPath(serverFolder);
        MsiSetProperty(hInstall, L"SERVER_DIRECTORY", serverFolder);
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall FixClientFolder(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "FixClientFolder");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    {
        CString clientFolder = GetProperty(hInstall, L"CLIENT_DIRECTORY");
        fixPath(clientFolder);
        MsiSetProperty(hInstall, L"CLIENT_DIRECTORY", clientFolder);
    }

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
