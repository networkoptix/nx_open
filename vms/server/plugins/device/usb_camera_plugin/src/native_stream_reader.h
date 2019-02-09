#pragma once

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

class NativeStreamReader: public AbstractStreamReader
{
public:
    NativeStreamReader(const std::shared_ptr<Camera>& camera);

    virtual int processPacket(
        const std::shared_ptr<ffmpeg::Packet>& source,
        std::shared_ptr<ffmpeg::Packet>& result) override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::shared_ptr<Camera> m_camera;
};

} // namespace usb_cam
} // namespace nx
