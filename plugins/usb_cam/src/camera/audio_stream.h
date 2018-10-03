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

        void addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
        void removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);

        int sampleRate() const;

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
        int m_retries = 0;

        bool m_terminated;
        mutable std::mutex m_mutex;
        std::condition_variable m_wait;
        std::thread m_runThread;

        std::unique_ptr<ffmpeg::Frame> m_decodedFrame;
        std::unique_ptr<ffmpeg::Frame> m_resampledFrame;
        struct SwrContext * m_resampleContext = nullptr;

        std::shared_ptr<std::atomic_int> m_packetCount;
        
        std::vector<std::shared_ptr<ffmpeg::Packet>> m_packetMergeBuffer;

        uint64_t m_lastTimeStamp = 0;
        int64_t m_lastPts = 0;

    private:
        std::string ffmpegUrl() const;
        void waitForConsumers();
        int initialize();
        void uninitialize();
        bool ensureInitialized();
        int initializeInputFormat();
        int initializeDecoder();
        int initializeEncoder();
        int initializeResampledFrame();
        int initalizeResampleContext(const AVFrame * frame);
        int decodeNextFrame(ffmpeg::Frame * outFrame);
        int resample(const ffmpeg::Frame * frame, ffmpeg::Frame * outFrame);
        std::chrono::milliseconds resampleDelay() const;
        std::shared_ptr<ffmpeg::Packet> nextPacket(int * outError);

        /**
         * Buffers the given packet to be merged later with packets given previously.
         * If enough packets have been given, then it merges all packets returning the merged packet.
         * @param[in] - the packet to buffer
         * @params[out] - outFfmpegError an error < 0 if one occured, 0 otherwise
         * @return - all previously given packets, merged into one
         */
        std::shared_ptr<ffmpeg::Packet> mergeAllPackets(const std::shared_ptr<ffmpeg::Packet>& packet, int * outFfmpegError);
        void start();
        void stop();
        void run();

        std::chrono::milliseconds timePerVideoFrame() const;
    };

public:
    AudioStream(const std::string url, const std::weak_ptr<Camera>& camera, bool enabled);
    ~AudioStream();
    
    std::string url() const;
    void setEnabled(bool enabled);
    bool enabled() const;

    void addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);

    int sampleRate() const;

private:
    std::string m_url;
    std::weak_ptr<Camera> m_camera;
    std::shared_ptr<PacketConsumerManager> m_packetConsumerManager;
    std::unique_ptr<AudioStreamPrivate> m_streamReader;
};



} //namespace usb_cam
} //namespace nx