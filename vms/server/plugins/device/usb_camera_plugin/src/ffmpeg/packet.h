#pragma once

#include <memory>
#include <atomic>

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx::usb_cam::ffmpeg {

class Packet
{
public:
    Packet(AVCodecID codecId, AVMediaType mediaType);
    ~Packet();

    int size() const;
    uint8_t * data();
    const uint8_t * data() const;
    int flags() const;
    int64_t pts() const;
    int64_t dts() const;
    AVPacket* packet() const;

    void initialize();
    void unreference();
    int newPacket(int size);

    AVCodecID codecId() const;
    AVMediaType mediaType() const;

    uint64_t timestamp() const;
    void setTimestamp(uint64_t millis);

    bool keyPacket() const;

    void copy(AVPacket& source);

private:
    AVCodecID m_codecId = AV_CODEC_ID_NONE;
    AVMediaType m_mediaType = AVMEDIA_TYPE_UNKNOWN;
    uint64_t m_timestamp = 0;
    AVPacket* m_packet = nullptr;
};

} // namespace nx::usb_cam::ffmpeg
