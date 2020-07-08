#include "shortcuts_win.h"

#include "stdafx.h"
#include "windows.h"
#include "winnls.h"
#include "shobjidl.h"
#include "objbase.h"
#include "objidl.h"
#include "shlguid.h"

#define _WIN32_DCOM
#pragma comment(lib, "ole32.lib")

#include <QtCore/QDir>

namespace {

    const int kMaxStringLength = 2048;

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
                if (hres == RPC_E_CHANGED_MODE) {
                    m_success = true;
                    m_needUninitialize = false;
                }
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

    /**
     * CreateLink                       Uses the Shell's IShellLink and IPersistFile interfaces
     *                                  to create and store a shortcut to the specified object.
     * \param lpszPathObj               Address of a buffer that contains the path of the object,
     *                                  including the file name.
     * \param lpszPathLink              Address of a buffer that contains the path where the
     *                                  Shell link is to be stored, including the file name.
     * \param lpszArgs                  Address of a buffer that contains parameters of the executable file.
     * \param lpszIconLocation          Address of a buffer that contains the location of the icon. Can be NULL.
     * \param iconIndex                 Index of an icon in the resource file.
     * \returns                         Result of calling the member functions of the interfaces.
     */
    HRESULT CreateLink(LPCWSTR lpszPathObj, LPCWSTR lpszPathLink, LPCWSTR lpszArgs, LPCWSTR lpszIconLocation = NULL, int iconIndex = 0)
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
            psl->SetArguments(lpszArgs);
            if (lpszIconLocation != NULL) {
                psl->SetIconLocation(lpszIconLocation, iconIndex);
            }

            // Query IShellLink for the IPersistFile interface, used for saving the
            // shortcut in persistent storage.
            hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

            if (SUCCEEDED(hres))
            {
              //  WCHAR wsz[MAX_PATH];

                // Ensure that the string is Unicode.
              //  MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH);

                // Add code here to check return value from MultiByteWideChar
                // for success.

                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(lpszPathLink, TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        return hres;
    }

    HRESULT GetLinkInfo(
        LPCWSTR lpszPathLink,
        LPWSTR lpszPathObj,
        LPWSTR lpszArgs,
        LPWSTR lpszIconLocation,
        int* iconIndex)
    {
        HRESULT hres;
        IShellLink* pShellLink;

        // Get a pointer to the IShellLink interface. It is assumed that CoInitialize
        // has already been called.
        hres = CoCreateInstance(
            CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,(LPVOID*)&pShellLink);
        if (!SUCCEEDED(hres))
            return hres;

        IPersistFile* pPersistFile;

        hres = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
        if (!SUCCEEDED(hres))
        {
            pShellLink->Release();
            return hres;
        }

        hres = pPersistFile->Load(lpszPathLink, STGM_READ);
        if (SUCCEEDED(hres))
        {
            pShellLink->GetPath(lpszPathObj, kMaxStringLength, NULL, 0);
            pShellLink->GetArguments(lpszArgs, kMaxStringLength);
            pShellLink->GetIconLocation(lpszIconLocation, kMaxStringLength, iconIndex);
        }

        pPersistFile->Release();
        pShellLink->Release();

        return hres;
    }

}

QnWindowsShortcuts::QnWindowsShortcuts(QObject *parent /* = NULL*/):
    QnPlatformShortcuts(parent)
{

}

wchar_t* qnStringToPWChar(const QString &value) {
    int len = value.length() + 1;
    wchar_t* result = new wchar_t[len];
    wmemset(result, 0, len);
    value.toWCharArray(result);
    return result;
}


bool QnWindowsShortcuts::createShortcut(const QString &sourceFile, const QString &destinationPath, const QString &name, const QStringList &arguments, int iconId) {
    QnComInitializer comInit;
    if (!comInit.success())
        return false;

    wchar_t* lpszSrcFile = qnStringToPWChar(sourceFile);

    QString fullPath = QDir::toNativeSeparators(destinationPath) + lit("\\") + name + lit(".lnk");

    wchar_t* lpszDstPath = qnStringToPWChar(fullPath);

    QString args = arguments.join(L' ');

    wchar_t* lpszArgs = qnStringToPWChar(args);

    wchar_t* lpszIconLocation = NULL;
    int iconIndex = 0;

    if (iconId != 0) {
        lpszIconLocation = qnStringToPWChar(sourceFile);

        /* To use icon ID instead of its index in the iIcon parameter, use the negative value of the icon ID. */
        iconIndex = -1 * iconId;
    }

    HRESULT rc = CreateLink(lpszSrcFile, lpszDstPath, lpszArgs, lpszIconLocation, iconIndex);

    delete[] lpszSrcFile;
    delete[] lpszDstPath;
    delete[] lpszArgs;
    delete[] lpszIconLocation;

    return SUCCEEDED(rc);
}

bool QnWindowsShortcuts::deleteShortcut(const QString &destinationPath, const QString &name) const {
    QString fullPath = QDir::toNativeSeparators(destinationPath) + lit("\\") + name + lit(".lnk");
    return QFile::remove(fullPath);
}

bool QnWindowsShortcuts::shortcutExists(const QString &destinationPath, const QString &name) const {
    QString fullPath = QDir::toNativeSeparators(destinationPath) + lit("\\") + name + lit(".lnk");
    return QFileInfo::exists(fullPath);
}

QnPlatformShortcuts::ShortcutInfo QnWindowsShortcuts::getShortcutInfo(
    const QString& destinationPath,
    const QString& name) const
{
    QnComInitializer comInit;
    if (!comInit.success())
        return {};

    QString fullPath = QDir::toNativeSeparators(destinationPath) + lit("\\") + name + lit(".lnk");
    wchar_t* lpszPathLink = qnStringToPWChar(fullPath);

    LPWSTR lpszPathObj = new wchar_t[kMaxStringLength];
    LPWSTR lpszArgs = new wchar_t[kMaxStringLength];
    LPWSTR lpszIconLocation = new wchar_t[kMaxStringLength];
    int iconIndex = 0;

    HRESULT rc = GetLinkInfo(lpszPathLink, lpszPathObj, lpszArgs, lpszIconLocation, &iconIndex);

    ShortcutInfo result;
    if (SUCCEEDED(rc))
    {
        result.sourceFile = QDir::fromNativeSeparators(
            QString::fromWCharArray(lpszPathObj, kMaxStringLength));

        result.arguments = QDir::fromNativeSeparators(
            QString::fromWCharArray(lpszArgs, kMaxStringLength))
                .split(L' ', QString::SkipEmptyParts);

        result.iconPath = QDir::fromNativeSeparators(
            QString::fromWCharArray(lpszIconLocation, kMaxStringLength));

        result.iconId = iconIndex;
    }

    delete[] lpszPathLink;
    delete[] lpszPathObj;
    delete[] lpszArgs;
    delete[] lpszIconLocation;

    return result;
}

bool QnWindowsShortcuts::supported() const {
    return true;
}
