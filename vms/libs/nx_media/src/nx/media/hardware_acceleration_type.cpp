// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QtGlobal>

#include "hardware_acceleration_type.h"
#include <nx/media/quick_sync/qsv_supported.h>

#if defined(__QSV_SUPPORTED__)

#include <nx/media/nvidia/nvidia_video_decoder.h>
#include <nx/media/quick_sync/quick_sync_video_decoder.h>

namespace nx::media {

HardwareAccelerationType getHardwareAccelerationType()
{
    if (nx::media::quick_sync::QuickSyncVideoDecoder::isAvailable())
        return HardwareAccelerationType::quickSync;
    else if (nx::media::nvidia::NvidiaVideoDecoder::isAvailable())
        return HardwareAccelerationType::nvidia;
    else
        return HardwareAccelerationType::ffmpeg;
}

} // namespace nx::media

#else

namespace nx::media {

HardwareAccelerationType getHardwareAccelerationType()
{
    return HardwareAccelerationType::ffmpeg;
}

} // namespace nx::media

#endif // defined(__QSV_SUPPORTED__)
