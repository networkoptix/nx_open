#pragma once

#include <memory>
#include <atomic>

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx {
namespace usb_cam {
namespace ffmpeg {

class Packet
{
public:
    Packet(
        AVCodecID codecId,
        AVMediaType mediaType,
        const std::shared_ptr<std::atomic_int>& packetCount = nullptr);
    ~Packet();

    int size() const;
    uint8_t * data();
    const uint8_t * data() const;
    int flags() const;
    int64_t pts() const;
    int64_t dts() const;
    AVPacket * packet() const;

    void initialize();
    void unreference();
    int newPacket(int size);
    
    AVCodecID codecId() const;
    AVMediaType mediaType() const;

    uint64_t timestamp() const;
    void setTimestamp(uint64_t millis);

    bool keyPacket() const;

private:
    AVCodecID m_codecId = AV_CODEC_ID_NONE;
    AVMediaType m_mediaType = AVMEDIA_TYPE_UNKNOWN;
    std::shared_ptr<std::atomic_int> m_packetCount;
    uint64_t m_timestamp = 0;
    AVPacket* m_packet = nullptr;

#ifdef _WIN32
private:
    mutable bool m_parseNalUnitsVisited = false;

private:
    void parseNalUnits() const;
#endif
};

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
