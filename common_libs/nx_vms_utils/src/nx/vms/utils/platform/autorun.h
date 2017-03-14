#pragma once

namespace nx {
namespace vms {
namespace utils {

NX_VMS_UTILS_API bool isAutoRunSupported();

NX_VMS_UTILS_API bool isAutoRunEnabled(const QString& key, const QString& path);
NX_VMS_UTILS_API void setAutoRunEnabled(const QString& key, const QString& path, bool value);

} // namespace utils
} // namespace vms
} // namespace nx
