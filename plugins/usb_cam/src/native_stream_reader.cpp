#include "native_stream_reader.h"

#include <memory>

#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

NativeStreamReader::NativeStreamReader(
    int encoderIndex,
    nxpl::TimeProvider *const timeProvider,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    InternalStreamReader(
        encoderIndex,
        timeProvider,
        codecParams,
        ffmpegStreamReader),
    m_initialized(false)
{
}

NativeStreamReader::~NativeStreamReader()
{
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if (!m_initialized)
    {
        m_ffmpegStreamReader->addConsumer(m_consumer);
        m_initialized = true;
    }

    maybeDropPackets();
    auto packet = m_consumer->popFront();

    if(m_interrupted)
    {
        m_interrupted = false;
        return nxcip::NX_INTERRUPTED;
    }

    /*!
     * Windows build of ffmpeg, or maybe just 3.1.9, doesn't set AV_PACKET_KEY_FLAG on packets produced
     * by av_read_frame(). To get around this, we must force the packet to be a key packet so the
     * client can attempt to decode it.
     */ 
#ifdef _WIN32
    bool forceKeyPacket = true;
#else
    bool forceKeyPacket = false;
#endif

    *lpPacket = toNxPacket(
        packet->packet(),
        packet->codecID(),
        packet->timeStamp() * 1000,
        forceKeyPacket).release();

    return nxcip::NX_NO_ERROR;
}

} // namespace usb_cam
} // namespace nx
