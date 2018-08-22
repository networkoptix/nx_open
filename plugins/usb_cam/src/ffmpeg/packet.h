#pragma once

#include <memory>
#include <atomic>

extern "C"{
#include <libavcodec/avcodec.h>
} // extern "C"

namespace nx {
namespace ffmpeg {

class Packet
{
public:
    Packet(AVCodecID codecID, const std::shared_ptr<std::atomic_int>& packetCount = nullptr);
    ~Packet();

    int size() const;
    uint8_t * data();
    const uint8_t * data() const;
    int flags() const;
    int64_t pts() const;
    AVPacket * packet() const;

    void setBuffer(uint8_t * data, int size, bool avMalloced);

    void initialize();
    void unreference();
    
    AVCodecID codecID() const;

    int copy(Packet * outPacket) const;

    uint64_t timeStamp() const;
    void setTimeStamp(uint64_t millis);

    bool keyPacket() const;

private:
    AVPacket* m_packet;
    AVCodecID m_codecID;
    std::shared_ptr<std::atomic_int> m_packetCount;
    uint64_t m_timeStamp;
};

} // namespace ffmpeg
} // namespace nx
