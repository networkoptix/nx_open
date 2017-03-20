#pragma once

#include <nx/utils/software_version.h>

namespace nx {
namespace vms {
namespace utils {

bool createDesktopFile(
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
