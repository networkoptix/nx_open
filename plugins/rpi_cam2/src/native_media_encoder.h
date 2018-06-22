#pragma once

#include "media_encoder.h"

namespace nx {
namespace webcam_plugin {

namespace ffmpeg { class StreamReader; }

class CameraManager;

class NativeMediaEncoder
:
    public MediaEncoder
{
public:
    NativeMediaEncoder(
        CameraManager* const cameraManager, 
        nxpl::TimeProvider *const timeProvider,
        const CodecContext& codecContext,
        const std::weak_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~NativeMediaEncoder();

    virtual nxcip::StreamReader* getLiveStreamReader() override;
};

} // namespace nx 
} // namespace webcam_plugin 
