// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hardware_acceleration_type.h"

#include <nx/media/supported_decoders.h>

#if NX_MEDIA_QUICK_SYNC_DECODER_SUPPORTED
    #include <nx/media/quick_sync/quick_sync_video_decoder.h>
#endif

#if NX_MEDIA_NVIDIA_DECODER_SUPPORTED
    #include <nx/media/nvidia/nvidia_video_decoder.h>
#endif

namespace nx::media {

HardwareAccelerationType getHardwareAccelerationType()
{
    #if NX_MEDIA_QUICK_SYNC_DECODER_SUPPORTED
        if (nx::media::quick_sync::QuickSyncVideoDecoder::isAvailable())
            return HardwareAccelerationType::quickSync;
    #endif

    #if NX_MEDIA_NVIDIA_DECODER_SUPPORTED
        if (nx::media::nvidia::NvidiaVideoDecoder::isAvailable())
            return HardwareAccelerationType::nvidia;
    #endif

    return HardwareAccelerationType::none;
}

} // namespace nx::media
