#pragma once
#include <atomic>

#include "camera/camera.h"
#include "ffmpeg/video_transcoder.h"

namespace nx::usb_cam {

class TranscodeStreamReader
{
public:
    TranscodeStreamReader(const std::shared_ptr<Camera>& camera);

    int transcode(
        const std::shared_ptr<ffmpeg::Packet>& source, std::shared_ptr<ffmpeg::Packet>& result);

    void setFps(float fps);
    void setResolution(const nxcip::Resolution& resolution);
    void setBitrate(int bitrate);

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
