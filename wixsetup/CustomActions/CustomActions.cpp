// CustomActions.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "Utils.h"

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

        portString = portString.Tokenize(L" ",curPos);
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

    static const int PORT_BASE = 7001;

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "FindFreePorts");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    int freePort1 = NextFreePort(PORT_BASE, PORT_BASE + 100);
    int freePort2 = NextFreePort(PORT_BASE + 100, PORT_BASE + 200);
    int freePort3 = NextFreePort(PORT_BASE + 200, PORT_BASE + 300);

    WCHAR portString[10];

    _itow_s(freePort1, portString, 10, 10);
    MsiSetProperty(hInstall, L"FREEPORT1", portString);

    _itow_s(freePort2, portString, 10, 10);
    MsiSetProperty(hInstall, L"FREEPORT2", portString);

    _itow_s(freePort3, portString, 10, 10);
    MsiSetProperty(hInstall, L"FREEPORT3", portString);

LExit:

    FinishWinsock();
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
