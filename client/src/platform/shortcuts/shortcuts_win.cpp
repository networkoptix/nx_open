#include "shortcuts_win.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include "stdafx.h"
#include "windows.h"
#include "winnls.h"
#include "shobjidl.h"
#include "objbase.h"
#include "objidl.h"
#include "shlguid.h"

#define _WIN32_DCOM
#pragma comment(lib, "ole32.lib")



namespace {

    class QnComInitializer {
    public:
        QnComInitializer():
        m_needUninitialize(true),
        m_success(false)
        {
            HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED); 
            m_success = SUCCEEDED(hres);
            if (!m_success)
            {
                if (hres == RPC_E_CHANGED_MODE)
                    m_needUninitialize = false;
                else {
                    qWarning() << "Failed to initialize COM library. Error code = 0x"  << hres;
                }
            }
        }

        ~QnComInitializer() {
            if (m_needUninitialize)
                CoUninitialize();
        }

        bool success() const {
            return m_success; 
        }
    private:
        bool m_needUninitialize;
        bool m_success;
    };

    // CreateLink - Uses the Shell's IShellLink and IPersistFile interfaces 
    //              to create and store a shortcut to the specified object. 
    //
    // Returns the result of calling the member functions of the interfaces. 
    //
    // Parameters:
    // lpszPathObj  - Address of a buffer that contains the path of the object,
    //                including the file name.
    // lpszPathLink - Address of a buffer that contains the path where the 
    //                Shell link is to be stored, including the file name.
    // lpszDesc     - Address of a buffer that contains a description of the 
    //                Shell link, stored in the Comment field of the link
    //                properties.
    HRESULT CreateLink(LPCWSTR lpszPathObj, LPCSTR lpszPathLink, LPCWSTR lpszDesc) 
    { 
        HRESULT hres; 
        IShellLink* psl; 

        // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
        // has already been called.
        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl); 
        if (SUCCEEDED(hres)) 
        { 
            IPersistFile* ppf; 

            // Set the path to the shortcut target and add the description. 
            psl->SetPath(lpszPathObj); 
            psl->SetDescription(lpszDesc); 

            // Query IShellLink for the IPersistFile interface, used for saving the 
            // shortcut in persistent storage. 
            hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf); 

            if (SUCCEEDED(hres)) 
            { 
                WCHAR wsz[MAX_PATH]; 

                // Ensure that the string is Unicode. 
                MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH); 

                // Add code here to check return value from MultiByteWideChar 
                // for success.

                // Save the link by calling IPersistFile::Save. 
                hres = ppf->Save(wsz, TRUE); 
                ppf->Release(); 
            } 
            psl->Release(); 
        } 
        return hres; 
    }


}

QnWindowsShortcuts::QnWindowsShortcuts(QObject *parent /*= NULL*/):
    QnPlatformShortcuts(parent)
{

}

bool QnWindowsShortcuts::createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments) {
    QnComInitializer comInit;
    if (!comInit.success())
        return false;

    wchar_t* lpszSrcFile = new wchar_t[sourceFile.length() + 1];
    sourceFile.toWCharArray(lpszSrcFile);

    wchar_t * lpszDstPath = new wchar_t[destinationPath.length() + 1];
    destinationPath.toWCharArray(lpszDstPath);

    wchar_t * lpszName = new wchar_t[name.length() + 1];
    name.toWCharArray(lpszName);

    HRESULT rc = CreateLink(lpszSrcFile, lpszDstPath, lpszName);

    delete[] lpszSrcFile;
    delete[] lpszDstPath;
    delete[] lpszName;

    return SUCCEEDED(rc);
}

bool QnWindowsShortcuts::shortcutExists(const QString &destinationPath, const QString &name) const 
{
    return false;
}
