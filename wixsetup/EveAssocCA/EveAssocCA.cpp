// EveAssocCA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "Registry.h"

static const LPWSTR EVE_MEDIA_PLAYER = L"EVE media player";

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
        if (FAILED(hr))
            throw Error("Failed to initialize");

        WcaLog(LOGMSG_STANDARD, "Initialized.");

        IApplicationAssociationRegistration *pAAR = NULL;

        hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 
            NULL,
            CLSCTX_INPROC,
            __uuidof(IApplicationAssociationRegistration),
            (void **)&pAAR);

        if (FAILED(hr))
            throw Error("CoCreateInstance failed");

        WcaLog(LOGMSG_STANDARD, "CoCreateInstance succeeded.");

        hr = pAAR->SetAppAsDefaultAll(EVE_MEDIA_PLAYER);

        if (FAILED(hr))
            throw Error("SetAppAsDefaultAll failed");

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

UINT __stdcall SaveFileAssociations(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    TCHAR* fileExtsString = 0;

    MessageBox(NULL,L"In",NULL,NULL);

    try
    {
        hr = WcaInitialize(hInstall, "SaveFileAssociations");
        if (FAILED(hr))
        {
            ExitTrace(hr, "Failed to initialize");
            throw Error("Failed to initialize");
        }

        WcaLog(LOGMSG_STANDARD, "Initialized.");

        fileExtsString = GetProperty(hInstall, TEXT("CustomActionData"));

        if (fileExtsString == NULL)
        {
            throw Error("SaveFileAssociations: FILETYPES is null.");
        }

        {
            IApplicationAssociationRegistration *pAAR = NULL;

            CoInitializeEx(NULL, COINIT_MULTITHREADED);

            hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 
                NULL,
                CLSCTX_INPROC,
                __uuidof(IApplicationAssociationRegistration),
                (void **)&pAAR);

            LPWSTR EVE_MEDIA_PLAYER = L"EVE media player";

            if (SUCCEEDED(hr)) {
                WcaLog(LOGMSG_STANDARD, "CoCreateInstance succeeded.");

                CAtlString fileExts(fileExtsString);

                CAtlString fileExt;
                int extPosition = 0;

                RegistryManager registryManager;

                fileExt = fileExts.Tokenize(_T("|"), extPosition);
                for (; fileExt != _T(""); fileExt = fileExts.Tokenize(_T("|"), extPosition))
                {
                    CAtlString dotFileExt(".");
                    CAtlString eve("EVE");
                    dotFileExt += fileExt;

                    LPWSTR association = NULL;
                    pAAR->QueryCurrentDefault(dotFileExt, AT_FILEEXTENSION, AL_MACHINE, &association);
                    if (association == NULL)
                        continue;

                    if (wcscmp(association, eve + dotFileExt))
                    {
                        CoTaskMemFree(association);
                        continue;
                    }

                    CoTaskMemFree(association);
                    pAAR->QueryCurrentDefault(dotFileExt, AT_FILEEXTENSION, AL_EFFECTIVE, &association);
                    if (wcscmp(association, eve + dotFileExt) == 0)
                    {
                        CoTaskMemFree(association);
                        continue;
                    }

                    // System association is EVE, effective is not EVE => backup existing, set our
                    CAtlString logMessage("Ext was selected: ");
                    logMessage += fileExt;
                    logMessage += " and associated with ";
                    logMessage += association;

                    CT2CA pszConvertedAnsiString(logMessage);
                    WcaLog(LOGMSG_STANDARD, pszConvertedAnsiString);

                    registryManager.backupAssociation(fileExt, association);

                    CoTaskMemFree(association);
                }

                hr = pAAR->SetAppAsDefaultAll(EVE_MEDIA_PLAYER);

                if (SUCCEEDED(hr)) {
                    WcaLog(LOGMSG_STANDARD, "SetAppAsDefaultAll succeeded.");
                } else {
                    WcaLog(LOGMSG_STANDARD, "SetAppAsDefaultAll failed.");
                }

                pAAR->Release();
            } else {
                WcaLog(LOGMSG_STANDARD, "CoCreateInstance failed.");
            }

            CoUninitialize();
        }
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

UINT __stdcall RestoreFileAssociations(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "RestoreFileAssociations");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

#if 0
    IApplicationAssociationRegistration *pAAR = NULL;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration, 
        NULL,
        CLSCTX_INPROC,
        __uuidof(IApplicationAssociationRegistration),
        (void **)&pAAR);

    if (SUCCEEDED(hr)) {
        WcaLog(LOGMSG_STANDARD, "CoCreateInstance succeeded.");

        hr = pAAR->SetAppAsDefaultAll(L"EVE media player");

        if (SUCCEEDED(hr)) {
            WcaLog(LOGMSG_STANDARD, "SetAppAsDefaultAll succeeded.");
        } else {
            WcaLog(LOGMSG_STANDARD, "SetAppAsDefaultAll failed.");
        }
        pAAR->Release();
    } else {
        WcaLog(LOGMSG_STANDARD, "CoCreateInstance failed.");
    }

    CoUninitialize();
#endif

LExit:
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
