// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shortcuts_win.h"

#include <winsock2.h>
#include <windows.h> //< You HAVE to include winsock2.h BEFORE windows.h.

#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <wchar.h>
#include <winnls.h>

#define _WIN32_DCOM
#pragma comment(lib, "ole32.lib")

#include <QtCore/QDir>

#include <nx/utils/string.h>

namespace {

constexpr int kMaxStringLength = 2048;

class QnComInitializer
{
public:
    QnComInitializer(): m_needUninitialize(true), m_success(false)
    {
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        m_success = SUCCEEDED(hres);
        if (!m_success)
        {
            if (hres == RPC_E_CHANGED_MODE)
            {
                m_success = true;
                m_needUninitialize = false;
            }
            else
            {
                qWarning() << "Failed to initialize COM library. Error code = 0x" << hres;
            }
        }
    }

    ~QnComInitializer()
    {
        if (m_needUninitialize)
            CoUninitialize();
    }

    bool success() const { return m_success; }

private:
    bool m_needUninitialize;
    bool m_success;
};

QString shortcutFullPath(const QString& destinationPath, const QString& name)
{
    return QDir::toNativeSeparators(destinationPath)
        + "\\"
        + nx::utils::replaceNonFileNameCharacters(name, '_')
        + ".lnk";
}

} // namespace

QnWindowsShortcuts::QnWindowsShortcuts(QObject* parent):
    QnPlatformShortcuts(parent)
{
}

bool QnWindowsShortcuts::createShortcut(
    const QString& sourceFile,
    const QString& destinationDir,
    const QString& name,
    const QStringList& arguments,
    const QVariant& icon)
{
    QnComInitializer comInit;
    if (!comInit.success())
        return false;

    IShellLink* pShellLink;
    if (!SUCCEEDED(CoCreateInstance(
        CLSID_ShellLink,
        /*Object is not being created as part of an aggregate*/ nullptr,
        /*The caller and the called code are in same process*/ CLSCTX_INPROC_SERVER,
        IID_IShellLink,
        (LPVOID*) &pShellLink)))
    {
        return false;
    }

    // Set the path to the shortcut target, add the description and, optionally, icon location.
    pShellLink->SetPath((LPCWSTR) sourceFile.data());
    const QString args = arguments.join(' ');
    pShellLink->SetArguments((LPCWSTR) args.data());
    if (!icon.isNull())
    {
        // Negative iIcon means that an icon id is used instead of an icon index.
        pShellLink->SetIconLocation((LPCWSTR) sourceFile.data(), /*iIcon*/ -icon.toInt());
    }

    IPersistFile* pPersistFile; //< Used for saving the shortcut in persistent storage.
    if (!SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*) &pPersistFile)))
    {
        pShellLink->Release();
        return false;
    }

    const QString fullPath = shortcutFullPath(destinationDir, name);
    pPersistFile->Save(
        (LPCWSTR) fullPath.data(),
        /*Make fullPath variable value a "current working file" for the created link*/ TRUE);

    // Release pointers to the interfaces.
    pPersistFile->Release();
    pShellLink->Release();

    return true;
}

bool QnWindowsShortcuts::deleteShortcut(const QString& destinationDir, const QString& name) const
{
    const QString fullPath = shortcutFullPath(destinationDir, name);
    return QFile::remove(fullPath);
}

bool QnWindowsShortcuts::shortcutExists(const QString& destinationDir, const QString& name) const
{
    const QString fullPath = shortcutFullPath(destinationDir, name);
    return QFileInfo::exists(fullPath);
}

QnPlatformShortcuts::ShortcutInfo QnWindowsShortcuts::getShortcutInfo(
    const QString& destinationDir,
    const QString& name) const
{
    ShortcutInfo result;

    QnComInitializer comInit;
    if (!comInit.success())
        return {};

    IShellLink* pShellLink;
    if (!SUCCEEDED(CoCreateInstance(
        CLSID_ShellLink,
        /*Object is not being created as part of an aggregate*/ nullptr,
        /*The caller and the called code are in same process*/ CLSCTX_INPROC_SERVER,
        IID_IShellLink,
        (LPVOID*) &pShellLink)))
    {
        return result;
    }

    IPersistFile* pPersistFile;
    if (!SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*) &pPersistFile)))
    {
        pShellLink->Release();
        return result;
    }

    const QString fullPath = QDir::toNativeSeparators(destinationDir) + "\\" + name + ".lnk";
    if (!SUCCEEDED(pPersistFile->Load((LPCWSTR) fullPath.data(), STGM_READ)))
    {
        pPersistFile->Release();
        pShellLink->Release();
        return result;
    }

    QString sourceFileString(kMaxStringLength, QChar(0));
    pShellLink->GetPath(
        (LPWSTR) sourceFileString.data(),
        kMaxStringLength,
        /*Pointer to the structure receiving additional information about the target*/ nullptr,
        /*fFlags*/ 0);
    nx::utils::truncateToNul(&sourceFileString);
    result.sourceFile = QDir::fromNativeSeparators(sourceFileString);

    QString linkArgsString(kMaxStringLength, QChar(0));
    pShellLink->GetArguments((LPWSTR) linkArgsString.data(), kMaxStringLength);
    nx::utils::truncateToNul(&linkArgsString);
    result.arguments = QDir::fromNativeSeparators(linkArgsString)
        .split(' ', Qt::SkipEmptyParts);

    QString iconLocationString(kMaxStringLength, QChar(0));
    int iconIndex = 0;
    pShellLink->GetIconLocation(
        (LPWSTR) iconLocationString.data(), kMaxStringLength, &iconIndex);
    nx::utils::truncateToNul(&iconLocationString);
    result.iconPath = QDir::fromNativeSeparators(iconLocationString);
    // Negative value means that an icon id is needed instead of an icon index.
    result.icon = QVariant(-iconIndex);

    pPersistFile->Release();
    pShellLink->Release();

    return result;
}

bool QnWindowsShortcuts::supported() const {
    return true;
}
