#pragma once

#include <string>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <map>

//#include "forward_declarations.h"
#include "ffmpeg/input_format.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"
#include "ffmpeg/frame.h"
#include "codec_parameters.h"
#include "timestamp_mapper.h"
#include "stream_consumer_manager.h"
#include "stream_consumer.h"

namespace nxpl { class TimeProvider; }

namespace nx {
namespace usb_cam {

//! Read a stream using ffmpeg from a camera input device
class VideoStreamReader
{
public:
    VideoStreamReader(
        const std::string& url,
        const CodecParameters& codecParams,
        nxpl::TimeProvider * const timeProvider);
    virtual ~VideoStreamReader();

    void addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
    void addFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer);
    void removeFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer);

    AVPixelFormat decoderPixelFormat() const;
    int gopSize() const;
    int fps() const;

    void updateFps();
    void updateBitrate();
    void updateResolution();

    std::string url() const;

private:
    enum CameraState
    {
        kOff,
        kInitialized,
        kModified
    };

    std::string m_url;
    CodecParameters m_codecParams;
    nxpl::TimeProvider *const m_timeProvider;

    CameraState m_cameraState;

    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    std::unique_ptr<ffmpeg::Codec> m_decoder;
    bool m_waitForKeyPacket;

    std::shared_ptr<std::atomic_int> m_packetCount;
    std::shared_ptr<std::atomic_int> m_frameCount;

    TimeStampMapper m_timeStamps;    

    FrameConsumerManager m_frameConsumerManager;
    PacketConsumerManager m_packetConsumerManager;

    std::thread m_videoThread;
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    bool m_terminated;
    int m_retries;
    int m_initCode;

private:
    std::string ffmpegUrl() const;
    bool consumersEmpty() const;
    void waitForConsumers();
    void updateFpsUnlocked();
    void updateResolutionUnlocked();
    void updateBitrateUnlocked();
    void updateUnlocked();
    void start();
    void stop();
    void run();
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int initializeInputFormat();
    void setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat);
    int initializeDecoder();
    std::shared_ptr<ffmpeg::Frame> maybeDecode(const ffmpeg::Packet * packet);
    int decode(const ffmpeg::Packet * packet, ffmpeg::Frame * frame);
};

} // namespace usb_cam
} // namespace nx
