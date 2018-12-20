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
#include "abstract_stream_consumer.h"

namespace nxpl { class TimeProvider; }

namespace nx {
namespace usb_cam {

class Camera;

class VideoStream
{
public:
    VideoStream(
        const std::weak_ptr<Camera>& camera,
        const CodecParameters& codecParams);
    virtual ~VideoStream();

    /**
     * Get the url used by ffmpeg to open the video stream.
     */
    std::string ffmpegUrl() const;
    
    /**
     * The target frames per second the video stream was opened with.
     */
    float fps() const;
    
    /**
     * The number of frames per second the video stream is actually producing, updated every
     *    second. Some cameras run at a lower fps than it actually reports.
     */
    float actualFps() const;

    /**
     * The amount of time it takes to produce a video frame at the target frame rate.
     */
    std::chrono::milliseconds timePerFrame() const;

    /**
     * The amount of time it takes to produce a video frame based on actualFps().
     */
    std::chrono::milliseconds actualTimePerFrame() const;

    AVPixelFormat decoderPixelFormat() const;

    void addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void addFrameConsumer(const std::weak_ptr<AbstractFrameConsumer>& consumer);
    void removeFrameConsumer(const std::weak_ptr<AbstractFrameConsumer>& consumer);

    void updateFps();
    void updateBitrate();
    void updateResolution();

    /**
     *  Return internal io error code, set if readFrame fails with such code.
     */
    bool ioError() const;

    /**
     * Queries hardware for the camera name to determine if the camera is plugged in.
     */
    bool pluggedIn() const;

private:
    enum CameraState
    {
        csOff,
        csInitialized,
        csModified
    };

    //std::string m_url;
    std::weak_ptr<Camera> m_camera;
    CodecParameters m_codecParams;
    nxpl::TimeProvider * const m_timeProvider;

    CameraState m_cameraState = csOff;

    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    std::unique_ptr<ffmpeg::Codec> m_decoder;
    std::atomic_int m_decoderPixelFormat = -1;
    bool m_skipUntilNextKeyPacket = true;

    std::atomic_int m_updatingFps = 0;
    std::atomic_int m_actualFps = 0;
    uint64_t m_oneSecondAgo = 0;

    /**
     * Some cameras crash the plugin if they are uninitialized while there are still packets and 
     * frames allocated. These variables are given to allocated packets and frames to keep track of 
     * the amount of frames and packets still waiting to be consumed. uninitialize() blocks until
     * both reach 0. All pointers to a packet or frame should be kept for as short as possible, 
     * assigning nullptr where possible to release decerement the count.
     */
    std::shared_ptr<std::atomic_int> m_packetCount;
    
    /**
     * See m_packetCount
     */
    std::shared_ptr<std::atomic_int> m_frameCount;

    TimestampMapper m_timestamps;    

    mutable std::mutex m_cameraMutex;
    std::condition_variable m_consumerWaitCondition;
    FrameConsumerManager m_frameConsumerManager;
    PacketConsumerManager m_packetConsumerManager;

    mutable std::mutex m_threadStartMutex;
    std::thread m_videoThread;
    std::atomic_bool m_terminated = true;
    std::atomic_bool m_ioError = false;
    int m_initCode = 0;

private:
    void updateActualFps(uint64_t now);
    /**
     * Get the url of the video stream, modified appropriately based on platform.
     */
    std::string ffmpegUrlPlatformDependent() const;
    bool waitForConsumers();
    bool noConsumers() const;
    void terminate();
    void tryToStartIfNotStarted();
    void start();
    void stop();
    void run();
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int initializeInputFormat();
    void setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat);
    int initializeDecoder();
    std::shared_ptr<ffmpeg::Packet> readFrame();
    std::shared_ptr<ffmpeg::Frame> maybeDecode(const ffmpeg::Packet * packet);
    int decode(const ffmpeg::Packet * packet, ffmpeg::Frame * frame);

    float findLargestFps() const;
    nxcip::Resolution findLargestResolution() const;
    int findLargestBitrate() const;
    void updateFpsUnlocked();
    void updateResolutionUnlocked();
    void updateBitrateUnlocked();
    void updateUnlocked();
    CodecParameters findClosestHardwareConfiguration(const CodecParameters& params) const;
    void setCodecParameters(const CodecParameters& codecParams);
    void setCameraState(CameraState cameraState);

    bool checkIoError(int ffmpegError);
    void setLastError(int ffmpegError);
};

} // namespace usb_cam
} // namespace nx
