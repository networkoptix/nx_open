#include "external_resources.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QResource>

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::utils {

QDir externalResourcesDirectory()
{
    if (nx::utils::AppInfo::isAndroid())
        return QDir("assets:/translations");

    const auto rootDir = QDir(QCoreApplication::applicationDirPath());
    // Final destination of external resources is different on macOS.
    if (nx::utils::AppInfo::isMacOsX())
    {
        auto resourcesDir = QDir(rootDir.absoluteFilePath("../Resources"));
        // There is no Resources directory in developer mode, avoid failing on the assert later.
        if (resourcesDir.exists())
            return resourcesDir;
    }
    return rootDir;
}

bool registerExternalResource(const QString& filename)
{
    const auto filePath = externalResourcesDirectory().absoluteFilePath(filename);
    NX_ASSERT(QFileInfo::exists(filePath), "Missing resource file %1", filePath);
    return QResource::registerResource(filePath);
}

bool registerExternalResourcesByMask(const QString& mask)
{
    const auto resourcesDir = externalResourcesDirectory();
    const auto filenames = resourcesDir.entryList(QStringList{mask});

    return std::all_of(filenames.cbegin(), filenames.cend(),
        [&resourcesDir](const auto& filename)
        {
            return QResource::registerResource(resourcesDir.absoluteFilePath(filename));
        });
}

} // namespace nx::vms::utils
