// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIODevice>

#include "core/resource/resource_fwd.h"

extern "C"
{
#include <libavformat/avio.h>

} // extern "C"

namespace nx::utils::media {

NX_VMS_COMMON_API AVIOContext* createFfmpegIOContext(
    QnStorageResourcePtr resource,
    const QString& url,
    QIODevice::OpenMode openMode,
    int ioBlockSize = 32768);

NX_VMS_COMMON_API AVIOContext* createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize = 32768);
NX_VMS_COMMON_API void closeFfmpegIOContext(AVIOContext* ioContext);

} // namespace nx::utils::media
