#pragma once

#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "ffmpeg/input_format.h"
#include "ffmpeg/audio_transcoder.h"

namespace nxpl { class TimeProvider; }

namespace nx::usb_cam {

class Camera;

class AudioStream
{
public:
    AudioStream(const std::string& url, nxpl::TimeProvider* timeProvider);
    ~AudioStream();

    int nextPacket(std::shared_ptr<ffmpeg::Packet>& packet);
    void uninitialize();
    void updateUrl(const std::string& url);

private:
    std::string ffmpegUrlPlatformDependent() const;
    int initialize();
    int initializeInput();
    uint64_t calculateTimestamp(int64_t duration);

private:
    mutable std::mutex m_mutex;
    std::string m_url;
    nxpl::TimeProvider* m_timeProvider;

    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    ffmpeg::AudioTranscoder m_transcoder;
    bool m_initialized = false;

    int64_t m_offsetTicks = 0;
    int64_t m_baseTimestamp = 0;
};

} // namespace nx::usb_cam
