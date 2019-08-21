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
    return nx::utils::AppInfo::isMacOsX()
        ? QDir(rootDir.absoluteFilePath("../Resources"))
        : rootDir;
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
