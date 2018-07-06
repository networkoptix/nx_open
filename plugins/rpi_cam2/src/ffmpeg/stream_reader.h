#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>

#include "forward_declarations.h"
#include "input_format.h"
#include "codec_parameters.h"
#include "codec.h"
#include "packet.h"

namespace nx {
namespace ffmpeg {

class StreamConsumer;

//! Read a stream using ffmpeg from a camera input device
class StreamReader
{
public:
    enum CameraState
    {
        kOff,
        kInitialized,
        kModified
    };

public:
    StreamReader(const char * url, const CodecParameters& codecParameters);
    virtual ~StreamReader();

    void addConsumer(const std::weak_ptr<StreamConsumer>& consumer);
    void removeConsumer(const std::weak_ptr<StreamConsumer>& consumer);

    CameraState cameraState() const;

    void updateFps();
    void updateBitrate();
    void updateResolution();

    //int decode(AVFrame * outframe, const AVPacket * packet);
private:
    std::string m_url;
    CodecParameters m_codecParams;

    //std::unique_ptr<Codec> m_decoder;
    std::unique_ptr<InputFormat> m_inputFormat;

    CameraState m_cameraState = kOff;

    std::vector<std::weak_ptr<StreamConsumer>> m_consumers;

    std::thread m_runThread;
    mutable std::mutex m_mutex;
    bool m_terminated = false;
    bool m_started = false;

private:
    void updateFpsUnlocked();
    void updateResolutionUnlocked();
    void updateBitrateUnlocked();
    void updateUnlocked();
    void start();
    void stop();
    void run();
    int readFrame(Packet * copyPacket = nullptr);
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
