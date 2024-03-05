// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "debug_helpers.h"

#include <QtCore/QDir>
#include <nx/utils/log/log.h>

namespace nx::utils::debug_helpers {

static nx::log::Tag kLogTag(QString("nx::utils::DebugHelpers"));

QString debugFilesDirectoryPath(const QString& path)
{
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);

    const QString basePath(nx::kit::IniConfig::iniFilesDir());
    const QString fullPath = basePath + path;

    const QDir directory(fullPath);
    if (!NX_ASSERT(directory.exists(), "Directory doesn't exist: %1", directory.absolutePath()))
        return QString();

    return directory.absolutePath();
}

} // namespace nx::utils::debug_helpers
