// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_info.h"

#include <QtCore/QStorageInfo>

namespace nx::log::detail {

// To mock for tests.
static VolumeInfoGetter g_volumeInfoGetter;

std::optional<VolumeInfo> getVolumeInfo(const QString& path)
{
    if (g_volumeInfoGetter)
        return g_volumeInfoGetter(path);

    QStorageInfo storageInfo(path);
    if (!storageInfo.isValid())
        return std::nullopt;

    // System wide free space, even if it's more
    // than available for the current OS user.
    return std::make_tuple(storageInfo.bytesTotal(), storageInfo.bytesFree());
}

void setVolumeInfoGetter(VolumeInfoGetter getter)
{
    g_volumeInfoGetter = std::move(getter);
}

} // namespace nx::log::detail
