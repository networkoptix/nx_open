#pragma once

#include <QtCore/QFlags>

#include <nx/utils/log/assert.h>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern "C"

/**
 * Here is the definition of the protocol used by testcamera to send a video stream to the Server.
 *
 * All classes in this unit have the byte layout corresponding to the protocol.
 *
 * Video stream is split into so-called packets. The packet contains the following data:
 * - Packet header, as defined in struct PacketHeader:
 *     - Byte with flags, as defined in enum PacketFlag.
 *     - Byte with ffmpeg's AVCodecID (codec ids > 255 are not supported).
 *     - Size of the packet data (depends on the packet type) in bytes, as 32-bit Big Endian. Can
 *         be up to 8 MB. ATTENTION: This is not the size of the packet body.
 * - Packet body, depending on the packet type and flags:
 *     - Media context packet (includes ffmpeg's MediaContext, has mediaContext flag set):
 *         - Media context data: serialized ffmpeg's MediaContext.
 *     - Frame packet (includes a single video frame, has codecContext flag not set):
 *         - If ptsIncluded flag set: frame's pts, as 64-bit Big Endian.
 *         - Frame data: raw compressed frame bytes.
 */
namespace nx::vms::testcamera::packet {
#pragma pack(push, 1)

/** Wrapper for integer values which internally stores the number as Big Endian. */
template<typename Value>
class BigEndian
{
    uint8_t m_bytes[sizeof(Value)]{};

public:
    BigEndian() = default;

    BigEndian(Value value)
    {
        for (size_t i = 0; i < sizeof(Value); ++i)
            m_bytes[i] = reinterpret_cast<const uint8_t*>(&value)[sizeof(Value) - 1 - i];
    }

    operator Value() const
    {
        Value value;
        for (size_t i = 0; i < sizeof(Value); ++i)
            reinterpret_cast<uint8_t*>(&value)[i] = m_bytes[sizeof(Value) - 1 - i];
        return value;
    }
};

enum class Flag: uint8_t
{
    none = 0,
    keyFrame = 1 << 7,
    mediaContext = 1 << 6, /**< Defines packet type: media context or frame. */
    ptsIncluded = 1 << 5, /**< Whether packet body includes 64-bit PTS. */
};
Q_DECLARE_FLAGS(Flags, Flag)
Q_DECLARE_OPERATORS_FOR_FLAGS(Flags)

/** Has the required binary layout, but offers getters/setters with convenient types and checks. */
class Header
{
    uint8_t m_flags{}; /** Flags (QFlags) presented as a single byte. */
    uint8_t m_codecId{}; /**< Ffmpeg's AVCodecID presented as a single byte. */
    BigEndian<uint32_t> m_dataSize{}; /**< Size of the packet data in bytes. */

public:
    Flags flags() const { return Flags(m_flags); }
    void setFlags(Flags v) { NX_ASSERT(v >= 0 && v < 256); m_flags = v; }
    QString flagsAsHex() const { return QString("0x%1").arg(m_flags, /*width*/ 2, /*base*/ 16); }

    AVCodecID codecId() const { return (AVCodecID) m_codecId; }
    void setCodecId(AVCodecID v) { NX_ASSERT(v > 0 && v < 256); m_codecId = v; }

    int dataSize() const { return m_dataSize; }
    void setDataSize(int v) { NX_ASSERT(v > 0 && v <= 1024 * 1024 * 8); m_dataSize = v; }
};
static_assert(sizeof(Header) == 6);

/** In the binary protocol, follows the Header of frame packet; optional. */
using PtsUs = BigEndian<int64_t>;
static_assert(sizeof(PtsUs) == 8);

#pragma pack(pop)
} // namespace nx::vms::testcamera::packet
