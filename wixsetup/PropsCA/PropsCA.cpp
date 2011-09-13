// PropsCA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <objbase.h>
#include <shobjidl.h>
#include <atlstr.h>
#include <shlobj.h>
#include <msxml2.h>

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

UINT __stdcall SetMoviesFolder(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "SetMoviesFolder");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    wchar_t moviesFolderPath[MAX_PATH];

    if (!SHGetSpecialFolderPath(0, moviesFolderPath, CSIDL_MYVIDEO, TRUE))
    {
        SHGetSpecialFolderPath(0, moviesFolderPath, CSIDL_MYDOCUMENTS, FALSE);
    }

    MsiSetProperty(hInstall, L"MOVIESFOLDER", moviesFolderPath);

LExit:
    
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}

UINT __stdcall SetProperties(MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "SetProperties");
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized.");

    TCHAR* strXmlConfigFile = GetProperty(hInstall, TEXT("XMLSETTINGS"));
    if (!strXmlConfigFile)
        goto LExit;

    TCHAR* strXmlConfigFileExpanded = new TCHAR[4096];

    ExpandEnvironmentStrings(strXmlConfigFile, strXmlConfigFileExpanded, 4096);

    IXMLDOMDocument * pXMLDoc;
    IXMLDOMNode * pXDN;
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ExitOnFailure(hr, "Failed to CoInitializeEx");

    hr = CoCreateInstance(__uuidof(DOMDocument), NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)&pXMLDoc);
    ExitOnFailure(hr, "Failed to CoCreateInstance");

    hr = pXMLDoc->QueryInterface(IID_IXMLDOMNode, (void **)&pXDN);
    ExitOnFailure(hr, "Failed to QueryInterface");

    VARIANT_BOOL isSuccessful = false;
    pXMLDoc->load(CComVariant(strXmlConfigFileExpanded), &isSuccessful);

    if (isSuccessful)
    {
        IXMLDOMNode* mediaRootNode;
        pXDN->selectSingleNode(L"/settings/config/mediaRoot", &mediaRootNode);

        BSTR mediaRoot;
        mediaRootNode->get_text(&mediaRoot);

        if (mediaRoot)
        {
            WcaLog(LOGMSG_STANDARD, "Old media folder is %s.", mediaRoot);
            MsiSetProperty(hInstall, L"MEDIAFOLDER", mediaRoot);
        }
    }

LExit:

    if (strXmlConfigFileExpanded)
        delete[] strXmlConfigFileExpanded;

    if (strXmlConfigFile)
        delete[] strXmlConfigFile;

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}