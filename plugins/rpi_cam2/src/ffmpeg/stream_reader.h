#pragma once

#include <string>
#include <memory>
#include <mutex>

#include "codec_context.h"
#include "forward_declarations.h"
#include "input_format.h"
#include "codec.h"

namespace nx {
namespace webcam_plugin {
namespace ffmpeg {

//! Read a stream using ffmpeg from some type of input
class StreamReader
{
public:
    StreamReader(const char * url, const CodecContext& codecParameters);
    virtual ~StreamReader();

    int addRef();
    int removeRef();

    const std::unique_ptr<Codec>& codec();
    const std::unique_ptr<InputFormat>& inputFormat();

    void setFps(int fps);
    void setBitrate(int bitrate);
    void setResolution(int width, int height);
    
    AVFrame * currentFrame();
    AVPacket * currentPacket();
    int loadNextData();

private:
    std::string m_url;
    CodecContext m_codecParams;

    std::unique_ptr<Codec> m_decoder;
    std::unique_ptr<InputFormat> m_inputFormat;

    AVPacket * m_currentPacket = nullptr;
    AVFrame * m_currentFrame = nullptr;

    enum CameraState
    {
        kOff,
        kInitialized,
        kModified
    };
    CameraState m_cameraState = kOff;
    std::mutex m_mutex;

    int m_refCount = 0;
private:
    bool ensureInitialized();
    int initialize();
    void uninitialize();
    int allocateCurrentPacket();
    int allocateCurrentFrame();
    void setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat);
    int decodeFrame(AVPacket * packet, AVFrame* outFrame);
};

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
