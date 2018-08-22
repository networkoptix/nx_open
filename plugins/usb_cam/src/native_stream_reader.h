#pragma once

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

//!Transfers or transcodes packets from USB webcameras and streams them
class NativeStreamReader : public StreamReaderPrivate
{
public:
    NativeStreamReader(
        int encoderIndex,
        const CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);
    virtual ~NativeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    virtual void interrupt() override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::shared_ptr<BufferedVideoPacketConsumer> m_consumer;

private:
    void ensureConsumerAdded() override;
    void maybeDropPackets();
    std::shared_ptr<ffmpeg::Packet> nextPacket(nxcip::DataPacketType * outMediaType);
};

} // namespace usb_cam
} // namespace nx
