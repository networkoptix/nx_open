#pragma once

#include "media_encoder.h"

namespace nx {
namespace usb_cam {

class CameraManager;

class NativeMediaEncoder: public MediaEncoder
{
public:
    NativeMediaEncoder(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderIndex,
        const std::shared_ptr<Camera>& camera);

    virtual ~NativeMediaEncoder() = default;

    virtual int getResolutionList(
        nxcip::ResolutionInfo * infoList,
        int * infoListCount) const override;

    virtual nxcip::StreamReader* getLiveStreamReader() override;

private:
    bool shouldStreamTranscode() const;
};

} // namespace nx
} // namespace usb_cam
