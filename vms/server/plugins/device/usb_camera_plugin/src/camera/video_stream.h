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
#include "stream_consumer_manager.h"
#include "abstract_stream_consumer.h"

namespace nxpl { class TimeProvider; }

namespace nx::usb_cam {

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

    void addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);
    void removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer);

    void setFps(float fps);
    void setResolution(const nxcip::Resolution& resolution);
    void setBitrate(int bitrate);

    /**
     *  Return internal io error code, set if readFrame fails with such code.
     */
    bool ioError() const;

    /**
     * Queries hardware for the camera name to determine if the camera is plugged in.
     */
    bool pluggedIn() const;

    AVCodecParameters* getCodecParameters();

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
    std::atomic<CameraState> m_streamState = csOff;
    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;

    mutable std::mutex m_mutex;
    PacketConsumerManager m_packetConsumerManager;

    mutable std::mutex m_threadStartMutex;
    std::thread m_videoThread;
    std::atomic_bool m_terminated = true;
    std::atomic_bool m_ioError = false;

private:
    /**
     * Get the url of the video stream, modified appropriately based on platform.
     */
    std::string ffmpegUrlPlatformDependent() const;
    void start();
    void stop();
    void run();
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int initializeInputFormat();
    void setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat);
    int readFrame(std::shared_ptr<ffmpeg::Packet>& result);
    CodecParameters findClosestHardwareConfiguration(const CodecParameters& params) const;
    void setCodecParameters(const CodecParameters& codecParams);

    bool checkIoError(int ffmpegError);
    void setLastError(int ffmpegError);
};

} // namespace nx::usb_cam
