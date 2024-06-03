// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>
#include <tuple>

#include <QtCore/QString>

namespace nx::log::detail {

using VolumeInfo = std::tuple<qint64, qint64>;
using VolumeInfoGetter = std::function<std::optional<VolumeInfo>(const QString&)>;

NX_UTILS_API std::optional<VolumeInfo> getVolumeInfo(const QString& path);

// Only for tests!
NX_UTILS_API void setVolumeInfoGetter(VolumeInfoGetter getter);
} // namespace nx::log::detail
