#pragma once

#include <QtCore/QString>
#include <nx/kit/ini_config.h>

namespace nx::utils::debug_helpers {

QString NX_UTILS_API debugFilesDirectoryPath(const QString& path, bool canCreate = true);

} // namespace nx::utils::debug_helpers
