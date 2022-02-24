// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/software_version.h>

namespace nx {
namespace vms {
namespace utils {

/** File name of the application icon (like vmsclient-default.png). */
QString NX_UTILS_API iconFileName();

bool NX_UTILS_API createDesktopFile(
    const QString& filePath,
    const QString& applicationBinaryPath,
    const QString& applicationName,
    const QString& description,
    const QString& icon,
    const nx::utils::SoftwareVersion& version = nx::utils::SoftwareVersion(),
    const QString& protocol = QString());

} // namespace utils
} // namespace vms
} // namespace nx
