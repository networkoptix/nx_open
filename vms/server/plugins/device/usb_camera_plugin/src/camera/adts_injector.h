#pragma once

#include <stdint.h>

struct AVFormatContext;
struct AVIOContext;
struct AVStream;

namespace nx {
namespace usb_cam {

namespace ffmpeg { class Packet; }
namespace ffmpeg { class Codec; }

class AdtsInjector
{
public:
    AdtsInjector() = default;
    ~AdtsInjector();
    int initialize(ffmpeg::Codec * codec);
    void uninitialize();
    int inject(ffmpeg::Packet * aacPacket);

    ffmpeg::Packet * currentPacket();

private:
    AVFormatContext * m_formatContext = nullptr;
    AVStream * m_outputStream = nullptr;
    AVIOContext * m_ioContext = nullptr;
    uint8_t * m_ioBuffer = nullptr;
    int m_ioBufferSize = 0;

    ffmpeg::Packet * m_currentPacket = nullptr;

private:
    int initializeIoContext(int bufferSize);
    void uninitializeIoContext();
    int reinitializeIoContext(int bufferSize);
};

}
}