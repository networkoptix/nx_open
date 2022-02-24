// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <nx/kit/ini_config.h>

namespace nx::utils::debug_helpers {

/** @return Empty string on assertion failure. */
QString NX_UTILS_API debugFilesDirectoryPath(const QString& path);

} // namespace nx::utils::debug_helpers
