#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "forward_declarations.h"
#include "input_format.h"
#include "codec.h"
#include "packet.h"

namespace nx {
namespace ffmpeg {

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

    struct CodecParameters
    {
        AVCodecID codecID;
        int fps;
        int bitrate;
        int width;
        int height;

        CodecParameters(AVCodecID codecID, int fps, int bitrate, int width, int height):
            codecID(codecID),
            fps(fps),
            bitrate(bitrate),
            width(width),
            height(height)
        {
        }

        void setResolution(int width, int height)
        {
            this->width = width;
            this->height = height;
        }
    };

public:
    StreamReader(const char * url, const CodecParameters& codecParameters);
    virtual ~StreamReader();
    
    int addRef();
    int releaseRef();

    CameraState cameraState() const;
    AVCodecID decoderID() const;

    const std::unique_ptr<Codec>& codec();
    const std::unique_ptr<InputFormat>& inputFormat();

    void setFps(int fps);
    void setBitrate(int bitrate);
    void setResolution(int width, int height);

    int loadNextData(Packet * copyPacket = nullptr);

    const std::unique_ptr<Packet>& currentPacket() const;
private:
    std::string m_url;
    CodecParameters m_codecParams;

    std::unique_ptr<Codec> m_decoder;
    std::unique_ptr<InputFormat> m_inputFormat;

    CameraState m_cameraState = kOff;
    std::mutex m_mutex;

    std::unique_ptr<Packet> m_currentPacket;

    int m_refCount = 0;
private:
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    void setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat);
};

} // namespace ffmpeg
} // namespace nx
