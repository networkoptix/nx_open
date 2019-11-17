#include "debug_helpers.h"

#include <QtCore/QDir>
#include <nx/utils/log/log.h>

namespace nx::utils::debug_helpers {

static nx::utils::log::Tag kLogTag(QString("nx::utils::DebugHelpers"));

QString debugFilesDirectoryPath(const QString& path, bool canCreate)
{
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);

    const QString basePath(nx::kit::IniConfig::iniFilesDir());
    const QString fullPath = basePath + path;

    const QDir directory(fullPath);
    if (canCreate)
    {
        if (!directory.exists())
        {
            if (!directory.mkpath(directory.absolutePath()))
            {
                NX_WARNING(kLogTag,
                    "Unable to create debug output dir: %1", directory.absolutePath());
                return QString();
            }
        }
    }
    else
    {
        if (!directory.exists())
        {
            NX_WARNING(kLogTag, "Debug output dir does not exist: %1", directory.absolutePath());
            return QString();
        }
    }

    return directory.absolutePath();
}

} // namespace nx::utils::debug_helpers
