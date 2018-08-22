#pragma once

#include "media_encoder.h"

namespace nx {
namespace usb_cam {

class CameraManager;

class NativeMediaEncoder
:
    public MediaEncoder
{
public:
    NativeMediaEncoder(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);

    virtual ~NativeMediaEncoder();

    virtual nxcip::StreamReader* getLiveStreamReader() override;
};

} // namespace nx 
} // namespace usb_cam 
