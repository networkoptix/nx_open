// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "external_resources.h"

#include <algorithm>
#include <set>

#include <QtCore/QCoreApplication>
#include <QtCore/QResource>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/thread/mutex.h>

namespace nx::utils {

// Counted list of registered directories. Each may be registered several times.
static std::multiset<QString> g_registeredDirectories;

/**
 * Avoid creating the mutex until other code needs it to allow ini tweaks to modify its
 * implementation.
 */
Mutex* resourceMutex()
{
    static Mutex mutex;
    return &mutex;
}

QDir externalResourcesDirectory()
{
    if (nx::build_info::isAndroid())
        return QDir("assets:/");

    const auto rootDir = QDir(QCoreApplication::applicationDirPath());
    // Final destination of external resources is different on macOS.
    if (nx::build_info::isMacOsX())
    {
        auto resourcesDir = QDir(rootDir.absoluteFilePath("../Resources"));
        // There is no Resources directory in developer mode, avoid failing on the assert later.
        if (resourcesDir.exists())
            return resourcesDir;
    }
    return rootDir;
}

bool registerExternalResource(const QString& filename, const QString& mapRoot)
{
    NX_MUTEX_LOCKER lock(resourceMutex());
    const auto filePath = externalResourcesDirectory().absoluteFilePath(filename);
    NX_ASSERT(QFileInfo::exists(filePath), "Missing resource file %1", filePath);
    return QResource::registerResource(filePath, mapRoot);
}

bool unregisterExternalResource(const QString& filename, const QString& mapRoot)
{
    NX_MUTEX_LOCKER lock(resourceMutex());
    const auto filePath = externalResourcesDirectory().absoluteFilePath(filename);
    NX_ASSERT(QFileInfo::exists(filePath), "Missing resource file %1", filePath);
    return QResource::unregisterResource(filePath, mapRoot);
}

bool registerExternalResourceDirectory(const QString& directory)
{
    NX_MUTEX_LOCKER lock(resourceMutex());
    g_registeredDirectories.insert(directory);
    if (g_registeredDirectories.count(directory) > 1)
        return true;

    QDir assetsDirectory(externalResourcesDirectory());
    if (!NX_ASSERT(assetsDirectory.cd(directory),
        "Unable to find '%1' in asset directory: %2", directory, assetsDirectory))
    {
        return false;
    }

    const auto filenames = assetsDirectory.entryList(QDir::Filter::Files);
    return std::all_of(filenames.cbegin(), filenames.cend(),
        [&assetsDirectory](const auto& filename)
        {
            const auto r = assetsDirectory.absoluteFilePath(filename);
            return NX_ASSERT(QResource::registerResource(r), "Unable to register asset: %1", r);
        });
}

bool unregisterExternalResourceDirectory(const QString& directory)
{
    NX_MUTEX_LOCKER lock(resourceMutex());
    auto node = g_registeredDirectories.extract(directory);
    if (g_registeredDirectories.contains(directory))
        return true; //< Directory still registered.

    if (node.empty())
        return true; // It is safe to unregister directory twice.

    QDir assetsDirectory(externalResourcesDirectory());
    if (!NX_ASSERT(assetsDirectory.cd(directory),
        "Unable to find '%1' in asset directory: %2", directory, assetsDirectory))
    {
        return false;
    }

    const auto filenames = assetsDirectory.entryList(QDir::Filter::Files);
    bool result = std::all_of(filenames.cbegin(), filenames.cend(),
        [&assetsDirectory](const auto& filename)
        {
            const auto r = assetsDirectory.absoluteFilePath(filename);
            return NX_ASSERT(
                QResource::unregisterResource(r),
                "Unable to unregister asset: %1",
                r);
        });

    return result;
}

} // namespace nx::utils
