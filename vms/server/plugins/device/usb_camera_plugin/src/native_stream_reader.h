#pragma once

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

class NativeStreamReader: public StreamReaderPrivate
{
public:
    NativeStreamReader(int encoderIndex, const std::shared_ptr<Camera>& camera);
    virtual ~NativeStreamReader();

    virtual int getNextData(nxcip::MediaDataPacket** packet) override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::shared_ptr<ffmpeg::Packet> nextPacket();
};

} // namespace usb_cam
} // namespace nx
