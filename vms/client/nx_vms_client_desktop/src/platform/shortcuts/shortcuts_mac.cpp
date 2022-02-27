// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/branding.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/core_foundation_mac/cf_url.h>
#include <nx/utils/platform/core_foundation_mac/cf_string.h>

#include "shortcuts_mac.h"

QnMacShortcuts::QnMacShortcuts(QObject* parent /* = nullptr*/):
    QnPlatformShortcuts(parent)
{
}

bool QnMacShortcuts::createShortcut(
    const QString& sourceFilePath,
    const QString& destinationDir,
    const QString& name,
    /*unused*/ const QStringList& /*arguments*/,
    /*unused*/ const QVariant& /*icon*/)
{
    CFErrorRef error = NULL;
    auto aliasFilePath = destinationDir + "/" + name;
    cf::QnCFUrl aliasFile = cf::QnCFUrl::createFileUrl(aliasFilePath);
    cf::QnCFUrl targetFile = cf::QnCFUrl::createFileUrl(sourceFilePath);
    CFDataRef bookmark = CFURLCreateBookmarkData(NULL, targetFile.ref(),
        kCFURLBookmarkCreationSuitableForBookmarkFile, NULL, NULL, &error);

    if (bookmark == NULL)
    {
        auto errorDescription = cf::QnCFString(CFErrorCopyDescription(error));

        NX_DEBUG(typeid(QnMacShortcuts), "Cannot create shortcut to %1: %2 (code: %3)",
            sourceFilePath,
            errorDescription.toString(),
            CFErrorGetCode(error));

        return false;
    }

    Boolean success = CFURLWriteBookmarkDataToFile(bookmark, aliasFile.ref(),
        kCFURLBookmarkCreationSuitableForBookmarkFile, &error);
    if (!success)
    {
        auto errorDescription = cf::QnCFString(CFErrorCopyDescription(error));

        NX_DEBUG(typeid(QnMacShortcuts), "Cannot write shortcut as file %1: %2 (code: %3)",
            aliasFilePath,
            errorDescription.toString(),
            CFErrorGetCode(error));

        return false;
    }

    if (bookmark)
        CFRelease(bookmark);

    return true;
}

bool QnMacShortcuts::deleteShortcut(const QString& destinationPath, const QString& name) const
{
    return true;
}

bool QnMacShortcuts::shortcutExists(const QString& destinationPath, const QString& name) const
{
    return false;
}

QnPlatformShortcuts::ShortcutInfo QnMacShortcuts::getShortcutInfo(
    const QString& destinationPath,
    const QString& name) const
{
    return {};
}

bool QnMacShortcuts::supported() const
{
    return false;
}
