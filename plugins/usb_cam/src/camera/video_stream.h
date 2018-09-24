#pragma once

#include <string>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include <deque>
#include <thread>
#include <atomic>
#include <map>

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

class Camera;

//! Read a stream using ffmpeg from a camera input device
class VideoStream
{
public:
    VideoStream(
        const std::string& url,
        const CodecParameters& codecParams,
        const std::weak_ptr<Camera>& camera);
    virtual ~VideoStream();

    std::string url() const;
    float fps() const;
    AVPixelFormat decoderPixelFormat() const;

    void addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer);
    void addFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer);
    void removeFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer);

    void updateFps();
    void updateBitrate();
    void updateResolution();
    
    float actualFps() const;

private:
    enum CameraState
    {
        csOff,
        csInitialized,
        csModified
    };

    std::string m_url;
    CodecParameters m_codecParams;
    std::weak_ptr<Camera> m_camera;
    nxpl::TimeProvider * const m_timeProvider;

    CameraState m_cameraState;

    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    std::unique_ptr<ffmpeg::Codec> m_decoder;
    bool m_waitForKeyPacket;

    std::atomic_int m_updatingFps;
    std::atomic_int m_actualFps;
    uint64_t m_oneSecondAgo;

    /**
     * Some cameras crash the plugin if they are uninitialized while there are still packets and 
     * frame allocated. These variables are given to allocated packets and frames tokeep track of 
     * the amount of frames and packets still waiting to be consumed. /a uninitialize() blocks until
     * both reach 0. All pointers to a packet or frame should be kept for as short as possible, 
     * assigning nullptr where possible to release decerement the count.
     */
    std::shared_ptr<std::atomic_int> m_packetCount;
    
    /**
     * See /a m_packetCount
     */
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
    void updateActualFps(uint64_t now);
    std::string ffmpegUrl() const;
    bool consumersEmpty() const;
    void waitForConsumers();
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

    float largestFps() const;
    void largestResolution(int * outWidth, int * outHeight) const;
    int largestBitrate() const;
    void updateFpsUnlocked();
    void updateResolutionUnlocked();
    void updateBitrateUnlocked();
    void updateUnlocked();
    CodecParameters closestHardwareConfiguration(const CodecParameters& params) const;
    void setCodecParameters(const CodecParameters& codecParams);
};

} // namespace usb_cam
} // namespace nx
