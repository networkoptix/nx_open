// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace utils {

NX_VMS_UTILS_API bool isAutoRunSupported();

NX_VMS_UTILS_API QString autoRunPath(const QString& key);
NX_VMS_UTILS_API bool isAutoRunEnabled(const QString& key);
NX_VMS_UTILS_API void setAutoRunEnabled(const QString& key, const QString& path, bool value);

} // namespace utils
} // namespace vms
} // namespace nx
