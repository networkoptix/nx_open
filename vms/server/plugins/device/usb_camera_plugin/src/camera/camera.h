#pragma once

#include <memory>
#include <atomic>
#include <vector>

#include <camera/camera_plugin.h>
#include <plugins/plugin_container_api.h>

#include "video_stream.h"
#include "audio_stream.h"
#include "packet_buffer.h"

namespace nx::usb_cam {

class Camera: public std::enable_shared_from_this<Camera>
{
public:
    Camera(
        const std::string& url,
        const nxcip::CameraInfo& cameraInfo,
        nxpl::TimeProvider* const timeProvider);

    void setCredentials(const char* username, const char* password);

    bool initialize();
    void uninitialize();
    VideoStream&  videoStream();
    AudioStream&  audioStream();

    bool hasAudio() const;
    void setAudioEnabled(bool value);
    bool audioEnabled() const;
    void setUrl(const std::string& url);
    int lastError() const;

    uint64_t millisSinceEpoch() const;

    const nxcip::CameraInfo& info() const;
    std::string toString() const;

    int nextPacket(std::shared_ptr<ffmpeg::Packet>& packet);
    int nextBufferedPacket(std::shared_ptr<ffmpeg::Packet>& packet);
    void enablePacketBuffering(bool value);
    void interruptBufferPacketWait();

private:
    static const std::vector<nxcip::CompressionType> kVideoCodecPriorityList;

    nxcip::CameraInfo m_cameraInfo;
    nxpl::TimeProvider* const m_timeProvider;
    CodecParameters m_defaultVideoParams;
    mutable std::mutex m_mutex;

    // Needed for emulating of secondary stream
    PacketBuffer m_packetBuffer;

    std::unique_ptr<AudioStream> m_audioStream;
    std::unique_ptr<VideoStream> m_videoStream;

    bool m_audioEnabled = false;
    std::atomic_bool m_keyFrameFound = false;
    std::atomic_bool m_enablePacketBuffering = false;
    std::atomic_int m_lastError = 0;

    bool m_initialized = false;
};

} // namespace nx::usb_cams
