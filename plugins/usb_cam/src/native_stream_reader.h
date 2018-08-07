#pragma once

#include "stream_reader.h"

namespace nx {
namespace usb_cam {

//!Transfers or transcodes packets from USB webcameras and streams them
class NativeStreamReader : public InternalStreamReader
{
public:
    NativeStreamReader(
        int encoderIndex,
        nxpl::TimeProvider *const timeProvider,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~NativeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    virtual void interrupt() override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::shared_ptr<ffmpeg::BufferedPacketConsumer> m_consumer;
    bool m_added;
    bool m_interrupted;

private:
    void ensureAdded();
    void maybeDropPackets();
};

} // namespace usb_cam
} // namespace nx
