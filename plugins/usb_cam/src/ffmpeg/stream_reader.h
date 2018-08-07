#pragma once

#include <string>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <map>

#include "forward_declarations.h"
#include "input_format.h"
#include "codec_parameters.h"
#include "codec.h"
#include "packet.h"
#include "frame.h"

namespace nxpl { class TimeProvider; }

namespace nx {
namespace ffmpeg {

class StreamConsumer;
class PacketConsumer;
class FrameConsumer;

//! Read a stream using ffmpeg from a camera input device
class StreamReader
{
public:
    StreamReader(
        const char * url,
        const CodecParameters& codecParams,
        nxpl::TimeProvider * const timeProvider);
    virtual ~StreamReader();

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
    std::unique_ptr<ffmpeg::Codec> m_decoder;

    std::shared_ptr<std::atomic_int> m_packetCount;
    std::shared_ptr<std::atomic_int> m_frameCount;

    std::map<int64_t/*AVPacket.pts*/, int64_t/*ffmpeg::Packet.timeStamp()*/> m_timeStamps;

    std::vector<std::weak_ptr<PacketConsumer>> m_packetConsumers;
    std::vector<std::weak_ptr<FrameConsumer>> m_frameConsumers;

    std::thread m_runThread;
    std::condition_variable m_wait;
    mutable std::mutex m_mutex;
    bool m_terminated;
    int m_retries;

private:
    std::string ffmpegUrl() const;
    void updateFpsUnlocked();
    void updateResolutionUnlocked();
    void updateBitrateUnlocked();
    void updateUnlocked();
    int packetConsumerIndex (const std::weak_ptr<PacketConsumer> &consumer);
    int frameConsumerIndex (const std::weak_ptr<FrameConsumer>& consumer);
    void start();
    void stop();
    void run();
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int initializeInputFormat();
    void setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat);
    int initializeDecoder();
    int decode(const Packet * packet, Frame * frame);
    void flush();
};

class StreamConsumer
{
public:
    virtual int fps() const = 0;
    virtual void resolution(int *width, int *height) const = 0;
    virtual int bitrate() const = 0;
    virtual void clear() = 0;
};

class PacketConsumer : public StreamConsumer
{
public:
    virtual void givePacket(const std::shared_ptr<Packet>& packet) = 0;
};

class FrameConsumer : public StreamConsumer
{
public:
    virtual void giveFrame(const std::shared_ptr<Frame>& frame) = 0;
};

} // namespace ffmpeg
} // namespace nx
