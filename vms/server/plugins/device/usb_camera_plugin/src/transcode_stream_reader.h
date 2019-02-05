#pragma once
#include <atomic>

#include "stream_reader.h"
#include "ffmpeg/video_transcoder.h"

namespace nx::usb_cam {

class TranscodeStreamReader: public AbstractStreamReader
{
public:
    TranscodeStreamReader(const std::shared_ptr<Camera>& camera);

    virtual int processPacket(
        const std::shared_ptr<ffmpeg::Packet>& source,
        std::shared_ptr<ffmpeg::Packet>& result) override;

    virtual void setFps(float fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;

private:
    std::shared_ptr<ffmpeg::Packet> nextPacket();
    int initializeTranscoder();

private:
    std::shared_ptr<Camera> m_camera;
    ffmpeg::VideoTranscoder m_transcoder;
    CodecParameters m_codecParams;
    std::atomic_bool m_encoderNeedsReinitialization = true;
};

} // namespace usb_cam::nx
