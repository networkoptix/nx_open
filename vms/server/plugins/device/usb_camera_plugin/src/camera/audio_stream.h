#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>

#include "stream_consumer_manager.h"
#include "timestamp_mapper.h"
#include "ffmpeg/input_format.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"
#include "adts_injector.h"

struct SwrContext;
namespace nxpl { class TimeProvider; }

namespace nx {
namespace usb_cam {

class Camera;

class AudioStream
{
private:
    class AudioStreamPrivate
    {
    public:
        AudioStreamPrivate(
            const std::string& url,
            const std::weak_ptr<Camera>& camera,
            const std::shared_ptr<PacketConsumerManager>& packetConsumerManager);
        ~AudioStreamPrivate();

        bool pluggedIn() const;
        bool ioError() const;

        void addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
        void removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);

    private:
        std::string m_url;
        std::weak_ptr<Camera> m_camera;
        nxpl::TimeProvider * const m_timeProvider;
        std::shared_ptr<PacketConsumerManager> m_packetConsumerManager;

        std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
        std::unique_ptr<ffmpeg::Codec> m_decoder;
        std::unique_ptr<ffmpeg::Codec> m_encoder;

        bool m_initialized = false;
        int m_initCode = 0;
        std::atomic_bool m_ioError = false;

        int64_t m_offsetTicks = 0;
        int64_t m_baseTimestamp = 0;

        std::atomic_bool m_terminated = true;
        mutable std::mutex m_mutex;
        std::condition_variable m_wait;
        std::thread m_runThread;

        std::unique_ptr<ffmpeg::Frame> m_decodedFrame;
        std::unique_ptr<ffmpeg::Frame> m_resampledFrame;
        struct SwrContext * m_resampleContext = nullptr;

        std::shared_ptr<std::atomic_int> m_packetCount;

        AdtsInjector m_adtsInjector;

    private:
        std::string ffmpegUrlPlatformDependent() const;
        bool waitForConsumers();
        int initialize();
        void uninitialize();
        bool ensureInitialized();
        int initializeInputFormat();
        int initializeDecoder();
        int initializeEncoder();
        int initializeResampledFrame();
        int initalizeResampleContext(const ffmpeg::Frame * frame);
        int decodeNextFrame(ffmpeg::Frame * outFrame);
        int resample(const ffmpeg::Frame * frame, ffmpeg::Frame * outFrame);
        std::chrono::milliseconds resampleDelay() const;
        int encode(const ffmpeg::Frame *frame, ffmpeg::Packet * outPacket);
        std::shared_ptr<ffmpeg::Packet> nextPacket(int * outError);
        uint64_t calculateTimestamp(int64_t duration);

        std::chrono::milliseconds timePerVideoFrame() const;

        bool checkIoError(int ffmpegError);
        void setLastError(int ffmpegError);
        void terminate();
        void tryStart();
        void start();
        void stop();
        void run();
    };

public:
    AudioStream(const std::string& url, const std::weak_ptr<Camera>& camera, bool enabled);
    ~AudioStream() = default;

    std::string url() const;
    void setEnabled(bool enabled);
    bool enabled() const;

    bool ioError() const;

    void addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);

private:
    std::string m_url;
    std::weak_ptr<Camera> m_camera;
    std::shared_ptr<PacketConsumerManager> m_packetConsumerManager;
    std::unique_ptr<AudioStreamPrivate> m_streamReader;
};



} //namespace usb_cam
} //namespace nx
