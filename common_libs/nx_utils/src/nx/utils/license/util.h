#pragma once

#include <QString>

namespace nx {
namespace utils {
namespace license {

QString NX_UTILS_API changedGuidByteOrder(const QString& guid);
int NX_UTILS_API hardwareIdVersion(const QString& hardwareId);

} // namespace license
} // namespace utils
} // namespace nx
