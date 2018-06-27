#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "codec_context.h"
#include "forward_declarations.h"
#include "input_format.h"
#include "codec.h"
#include "packet.h"

namespace nx {
namespace webcam_plugin {
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

public:
    StreamReader(const char * url, const CodecContext& codecParameters);
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
    CodecContext m_codecParams;

    std::unique_ptr<Codec> m_decoder;
    std::unique_ptr<InputFormat> m_inputFormat;

    CameraState m_cameraState = kOff;
    std::mutex m_mutex;

    std::unique_ptr<Packet> m_currentPacket;
    AVFrame * m_currentFrame = nullptr;

    int m_refCount = 0;
private:
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    void setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat);
    int decode(AVPacket * packet, AVFrame* outFrame);
};

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
