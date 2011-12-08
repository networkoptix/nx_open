// EveAssocCA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "Registry.h"

static const LPWSTR EVE_MEDIA_PLAYER = L"EVE media player";

#define CHECK(x, msg) if (FAILED(x)) { throw Error(msg); }

TCHAR* GetProperty(MSIHANDLE hInstall, LPCWSTR name)
{
    TCHAR* szValueBuf = NULL;

    DWORD dwSize=0;
    UINT uiStat = MsiGetProperty(hInstall, name, TEXT(""), &dwSize);
    if (ERROR_MORE_DATA == uiStat && dwSize != 0)
    {
        dwSize++; // add 1 for terminating '\0'
        szValueBuf = new TCHAR[dwSize];
        uiStat = MsiGetProperty(hInstall, name, szValueBuf, &dwSize);
    }

    if (ERROR_SUCCESS != uiStat)
    {
        if (szValueBuf != NULL) 
            delete[] szValueBuf;

        return NULL;
    }

    return szValueBuf;
}

#if 0
UINT __stdcall PopErrorMessage(MSIHANDLE hInstall){    LONG lResult;           #ifdef _DEBUG       DebugBreak();        #endif
MessageBox(NULL,"In",NULL,NULL);    MessageBox(NULL,"Out",NULL,NULL);          
return ERROR_SUCCESS;}
#endif 

UINT __stdcall SetDefaultAssociations(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    try
    {
        hr = WcaInitialize(hInstall, "SetDefaultAssociations");
        CHECK(hr, "Failed to initialize");

        WcaLog(LOGMSG_STANDARD, "Initialized.");

        IApplicationAssociationRegistration *pAAR = NULL;

        hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 
            NULL,
            CLSCTX_INPROC,
            __uuidof(IApplicationAssociationRegistration),
            (void **)&pAAR);

        if (FAILED(hr))
        {
            // No IApplicationAssociationRegistration implementation. Assume we're on Windows XP.
            hr = S_OK;
            throw Error("CoCreateInstance failed.");
        }

        WcaLog(LOGMSG_STANDARD, "CoCreateInstance succeeded.");

        hr = pAAR->SetAppAsDefaultAll(EVE_MEDIA_PLAYER);
        CHECK(hr, "SetAppAsDefaultAll failed");

        WcaLog(LOGMSG_STANDARD, "SetAppAsDefaultAll succeeded.");

        pAAR->Release();
    } catch (const Error& e)
    {
        std::ostringstream os;
        os << "SetDefaultAssociations(): " << e.what();
        WcaLogError(hr, os.str().c_str());
    }

    CoUninitialize();

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall SaveAssociations(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    TCHAR* fileExtsString = 0;

    try
    {
        hr = WcaInitialize(hInstall, "SaveAssociations");
        CHECK(hr, "Failed to initialize");

        WcaLog(LOGMSG_STANDARD, "Initialized.");

        fileExtsString = GetProperty(hInstall, TEXT("CustomActionData"));
        if (fileExtsString == NULL)
            throw Error("SaveFileAssociations: FILETYPES is null.");

        IApplicationAssociationRegistration *pAAR = NULL;

        CoInitializeEx(NULL, COINIT_MULTITHREADED);

        hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 
            NULL,
            CLSCTX_INPROC,
            __uuidof(IApplicationAssociationRegistration),
            (void **)&pAAR);

        if (FAILED(hr))
        {
            // No IApplicationAssociationRegistration implementation. Assume we're on Windows XP.
            hr = S_OK;
            throw Error("CoCreateInstance failed.");
        }

        LPWSTR EVE_MEDIA_PLAYER = L"EVE media player";

        WcaLog(LOGMSG_STANDARD, "CoCreateInstance succeeded.");

        RegistryManager registryManager;

        CAtlString fileExts(fileExtsString);
            
        CAtlString fileExt;
        int extPosition = 0;
        
        fileExt = fileExts.Tokenize(_T("|"), extPosition);
        for (; fileExt != _T(""); fileExt = fileExts.Tokenize(_T("|"), extPosition))
        {
            if (!registryManager.isExtSupported(fileExt))
                continue;

            CAtlString eve("EVE");

            LPWSTR association = NULL;
            pAAR->QueryCurrentDefault(fileExt, AT_FILEEXTENSION, AL_EFFECTIVE, &association);
            if (wcscmp(association, eve + fileExt) == 0)
            {
                CoTaskMemFree(association);
                continue;
            }

            // System association is EVE, effective is not EVE => backup existing, set to our
            CAtlString logMessage("Ext was selected: ");
            logMessage += fileExt;
            logMessage += " and associated with ";
            logMessage += association;

            CT2CA pszConvertedAnsiString(logMessage);
            WcaLog(LOGMSG_STANDARD, pszConvertedAnsiString);

            registryManager.setBackupAssociation(fileExt, association);

            CoTaskMemFree(association);
        }


        hr = pAAR->SetAppAsDefaultAll(EVE_MEDIA_PLAYER);
        CHECK(hr, "SetAppAsDefaultAll failed.");

        WcaLog(LOGMSG_STANDARD, "SetAppAsDefaultAll succeeded.");

        pAAR->Release();

        CoUninitialize();
    }
    catch (const Error& e)
    {
        WcaLog(LOGMSG_STANDARD, e.what());
    }

    if (fileExtsString != NULL)
    {
        delete[] fileExtsString;
    }

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall RestoreAssociations(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    TCHAR* fileExtsString = 0;

    // MessageBox(NULL,L"In",NULL,NULL);

    try
    {
        hr = WcaInitialize(hInstall, "RestoreAssociations");
        CHECK(hr, "Failed to initialize");

        WcaLog(LOGMSG_STANDARD, "Initialized.");

        RegistryManager registryManager;

        fileExtsString = GetProperty(hInstall, TEXT("CustomActionData"));
        if (fileExtsString == NULL)
            throw Error("RestoreAssociations: FILETYPES is null.");

        IApplicationAssociationRegistration *pAAR = NULL;

        CoInitializeEx(NULL, COINIT_MULTITHREADED);

        hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 
            NULL,
            CLSCTX_INPROC,
            __uuidof(IApplicationAssociationRegistration),
            (void **)&pAAR);

        if (FAILED(hr))
        {
            // No IApplicationAssociationRegistration implementation. Assume we're on Windows XP.
            hr = S_OK;
            throw Error("CoCreateInstance failed.");
        }

        ProgIdDictionary progIdDictionary;

        CAtlString fileExts(fileExtsString);

        CAtlString fileExt;
        int extPosition = 0;

        fileExt = fileExts.Tokenize(_T("|"), extPosition);
        for (; fileExt != _T(""); fileExt = fileExts.Tokenize(_T("|"), extPosition))
        {
            if (!registryManager.isExtSupported(fileExt))
                continue;

            CAtlString backupProgId = registryManager.getBackupAssociation(fileExt);
            CAtlString backupApp = progIdDictionary.appNameFromProgId(backupProgId);
            HRESULT hr = pAAR->SetAppAsDefault(backupApp, fileExt, AT_FILEEXTENSION);
        }

        pAAR->Release();

        CoUninitialize();
    }
    catch (const Error& e)
    {
        WcaLog(LOGMSG_STANDARD, e.what());
    }

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
