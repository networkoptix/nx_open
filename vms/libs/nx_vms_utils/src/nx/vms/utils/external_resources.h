#pragma once

#include <QtCore/QDir>
#include <QtCore/QString>

namespace nx::vms::utils {

NX_VMS_UTILS_API QDir externalResourcesDirectory();

NX_VMS_UTILS_API bool registerExternalResource(const QString& filename);

NX_VMS_UTILS_API bool registerExternalResourcesByMask(const QString& mask);

} // namespace nx::vms::utils
