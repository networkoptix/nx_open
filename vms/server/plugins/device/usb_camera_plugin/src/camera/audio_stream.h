#pragma once

#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "stream_consumer_manager.h"
#include "ffmpeg/input_format.h"
#include "ffmpeg/audio_transcoder.h"

namespace nxpl { class TimeProvider; }

namespace nx::usb_cam {

class Camera;

class AudioStream
{
public:
    AudioStream(const std::string& url, const std::weak_ptr<Camera>& camera, bool enabled);
    ~AudioStream();

    void setEnabled(bool enabled);
    bool enabled() const;
    bool ioError() const;

    void addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);

private:
    std::string ffmpegUrlPlatformDependent() const;
    int initialize();
    void uninitialize();
    bool ensureInitialized();
    int initializeInputFormat();
    uint64_t calculateTimestamp(int64_t duration);
    int nextPacket(std::shared_ptr<ffmpeg::Packet>& packet);

    bool checkIoError(int ffmpegError);
    void setLastError(int ffmpegError);
    void start();
    void stop();
    void run();


private:
    std::string m_url;
    std::weak_ptr<Camera> m_camera;
    nxpl::TimeProvider* const m_timeProvider;
    PacketConsumerManager m_packetConsumerManager;
    mutable std::mutex m_mutex;

    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    ffmpeg::AudioTranscoder m_transcoder;

    bool m_initialized = false;
    std::atomic_bool m_ioError = false;
    std::atomic_bool m_terminated = true;
    std::atomic_bool m_enabled = false;
    std::thread m_audioThread;

    int64_t m_offsetTicks = 0;
    int64_t m_baseTimestamp = 0;
};

} // namespace nx::usb_cam
