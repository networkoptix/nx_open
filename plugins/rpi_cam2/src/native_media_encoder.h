#pragma once

#include "media_encoder.h"

namespace nx {
namespace rpi_cam2 {

class CameraManager;

class NativeMediaEncoder
:
    public MediaEncoder
{
public:
    NativeMediaEncoder(
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        CameraManager* const cameraManager, 
        nxpl::TimeProvider *const timeProvider,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~NativeMediaEncoder();

    virtual nxcip::StreamReader* getLiveStreamReader() override;
};

} // namespace nx 
} // namespace rpi_cam2 
