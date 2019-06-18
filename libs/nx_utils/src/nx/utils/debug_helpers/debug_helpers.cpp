#include "debug_helpers.h"

#include <QtCore/QDir>
#include <nx/utils/log/log.h>

namespace nx::utils::debug_helpers {

static nx::utils::log::Tag kLogTag(QString("nx::utils::DebugHelpers"));

QString debugFilesDirectoryPath(const QString& path)
{
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);

    const QString basePath(nx::kit::IniConfig::iniFilesDir());
    const QString fullPath = basePath + path;

    const QDir directory(fullPath);
    if (!directory.exists())
    {
        NX_WARNING(kLogTag, "Directory doesn't exist: %1", directory.absolutePath());
        return QString();
    }

    return directory.absolutePath();
}

} // namespace nx::utils::debug_helpers
