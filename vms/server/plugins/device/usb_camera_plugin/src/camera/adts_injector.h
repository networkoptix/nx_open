#pragma once

#include <stdint.h>

struct AVStream;
struct AVIOContext;
struct AVFormatContext;

namespace nx::usb_cam {

namespace ffmpeg { class Codec; }
namespace ffmpeg { class Packet; }

class AdtsInjector
{
public:
    AdtsInjector() = default;
    ~AdtsInjector();
    int initialize(ffmpeg::Codec * codec);
    void uninitialize();
    int inject(ffmpeg::Packet * aacPacket);

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
    static int writePacket(void * opaque, uint8_t *buffer, int bufferSize);
};

} // namespace nx::usb_cam

