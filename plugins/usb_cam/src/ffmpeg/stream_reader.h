#pragma once

#include <string>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>

#include "forward_declarations.h"
#include "input_format.h"
#include "codec_parameters.h"
#include "codec.h"
#include "packet.h"

namespace nxpl { class TimeProvider; }

namespace nx {
namespace ffmpeg {

class StreamConsumer;

//! Read a stream using ffmpeg from a camera input device
class StreamReader
{
public:
    StreamReader(
        const char * url,
        const CodecParameters& codecParams,
        nxpl::TimeProvider * const timeProvider);
    virtual ~StreamReader();

    void addConsumer(const std::weak_ptr<StreamConsumer>& consumer);
    void removeConsumer(const std::weak_ptr<StreamConsumer>& consumer);
    int initializeDecoderFromStream(Codec* decoder) const;

    int gopSize() const;
    int fps() const;

    void updateFps();
    void updateBitrate();
    void updateResolution();

    int lastFfmpegError() const;

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
    int m_lastFfmpegError;

    std::unique_ptr<InputFormat> m_inputFormat;

    std::vector<std::weak_ptr<StreamConsumer>> m_consumers;

    std::thread m_runThread;
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    bool m_terminated;

private:
    std::string ffmpegUrl() const;
    void updateFpsUnlocked();
    void updateResolutionUnlocked();
    void updateBitrateUnlocked();
    void updateUnlocked();
    int consumerIndex (const std::weak_ptr<StreamConsumer> &consumer);
    void start();
    void stop();
    void run();
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    void setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat);
};

class StreamConsumer
{
public:
    virtual int fps() const = 0;
    virtual void resolution(int *width, int *height) const = 0;
    virtual int bitrate() const = 0;
    virtual void givePacket(const std::shared_ptr<Packet>& packet) = 0;
};

} // namespace ffmpeg
} // namespace nx
