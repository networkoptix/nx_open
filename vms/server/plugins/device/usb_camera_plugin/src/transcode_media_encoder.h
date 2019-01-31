#pragma once

#include "media_encoder.h"

namespace nx {
namespace usb_cam {

class CameraManager;

class TranscodeMediaEncoder: public MediaEncoder
{
public:
    TranscodeMediaEncoder(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderIndex,
        const std::shared_ptr<Camera>& camera);

    virtual ~TranscodeMediaEncoder() = default;

    virtual int getResolutionList(
        nxcip::ResolutionInfo* infoList,
        int* infoListCount) const override;
    virtual int setFps(const float& fps, float*selectedFps) override;

    virtual nxcip::StreamReader* getLiveStreamReader() override;

private:
    CodecParameters calculateSecondaryCodecParams(
        const std::vector<device::video::ResolutionData>& resolutionList) const;
};

} // namespace nx
} // namespace usb_cam
