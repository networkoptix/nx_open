#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "camera/stream_consumer_manager.h"
#include "timestamp_mapper.h"
#include "ffmpeg/input_format.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"

namespace nxpl { class TimeProvider; }

struct SwrContext;

namespace nx {
namespace usb_cam {

class AudioStreamReader
{
private:
    class AudioStreamReaderPrivate
    {
    public:
        AudioStreamReaderPrivate(
            const std::string& url,
            nxpl::TimeProvider * timeProvider,
            const std::shared_ptr<PacketConsumerManager>& packetConsumerManager);
        ~AudioStreamReaderPrivate();

        void addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
        void removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);

        int sampleRate() const;

    private:    
        std::string m_url;
        nxpl::TimeProvider * m_timeProvider;
        std::shared_ptr<PacketConsumerManager> m_packetConsumerManager;

        std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
        std::unique_ptr<ffmpeg::Codec> m_decoder;
        std::unique_ptr<ffmpeg::Codec> m_encoder;

        bool m_initialized;
        int m_initCode;
        int m_retries;

        bool m_terminated;
        mutable std::mutex m_mutex;
        std::condition_variable m_wait;
        std::thread m_runThread;

        std::unique_ptr<ffmpeg::Frame> m_resampledFrame;
        struct SwrContext * m_resampleContext;

        std::shared_ptr<std::atomic_int> m_packetCount;
        TimeStampMapper m_timeStamps;

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
        int decodeNextFrame(AVFrame * outFrame);
        int resample(const AVFrame * frame, AVFrame * outFrame);
        std::shared_ptr<ffmpeg::Packet> getNextData(int * outError);
        void start();
        void stop();
        void run();
    };

public:
    AudioStreamReader(const std::string url, nxpl::TimeProvider* timeProvider, bool enabled);
    ~AudioStreamReader();
    
    std::string url() const;
    void setEnabled(bool enabled);
    bool enabled() const;

    void addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);

    int sampleRate() const;

private:
    std::string m_url;
    nxpl::TimeProvider * const m_timeProvider;
    std::shared_ptr<PacketConsumerManager> m_packetConsumerManager;
    std::unique_ptr<AudioStreamReaderPrivate> m_streamReader;
};



} //namespace usb_cam
} //namespace nx