#pragma once

#include "media_encoder.h"

namespace nx {
namespace webcam_plugin {

class CameraManager;

class TranscodeMediaEncoder
:
    public MediaEncoder
{
public:
    TranscodeMediaEncoder(
        CameraManager* const cameraManager, 
        nxpl::TimeProvider *const timeProvider,
        const CodecContext& codecContext,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);

    virtual ~TranscodeMediaEncoder();

    virtual nxcip::StreamReader* getLiveStreamReader() override;
};

} // namespace nx 
} // namespace webcam_plugin 
