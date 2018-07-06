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

//! Read a stream using ffmpeg from some type of input
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
    void start();

    CameraState cameraState() const;

    const std::unique_ptr<Codec>& decoder();
    const std::unique_ptr<InputFormat>& inputFormat();

    void setFps(int fps);
    void setBitrate(int bitrate);
    void setResolution(int width, int height);

private:
    std::string m_url;
    CodecParameters m_codecParams;

    std::unique_ptr<Codec> m_decoder;
    std::unique_ptr<InputFormat> m_inputFormat;

    CameraState m_cameraState = kOff;
    mutable std::mutex m_mutex;

    std::vector<std::weak_ptr<StreamConsumer>> m_consumers;

    bool m_started = false;
    bool m_terminated = false;
private:
    void run();
    int loadNextData(Packet * copyPacket = nullptr);
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    void setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat);
    int decode(AVFrame * outframe, const AVPacket * packet);
    void wait();
};

class StreamConsumer : public std::enable_shared_from_this<StreamConsumer>
{
public:
    StreamConsumer(const std::weak_ptr<StreamReader>& streamReader):
        m_streamReader(streamReader)
    {
    }

    ~StreamConsumer()
    {
        if (auto sr = m_streamReader.lock())
            sr->removeConsumer(weak_from_this());
    }
    
    void initialize()
    {
        if(m_ready)
            return;

        m_ready = true;
        if(auto sr = m_streamReader.lock())
            sr->addConsumer(weak_from_this());
    }

    virtual int fps() const = 0;
    virtual void resolution(int * width, int * height) const = 0;
    virtual int bitrate() const = 0;
    virtual void givePacket(const std::shared_ptr<Packet>& packet) = 0;

protected:
    bool m_ready = false;
    std::weak_ptr<StreamReader> m_streamReader;
};

} // namespace ffmpeg
} // namespace nx
