#include "stream_reader.h"

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

#include "utils/utils.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/buffered_stream_consumer.h"

namespace nx {
namespace usb_cam {

StreamReader::StreamReader(
    int encoderIndex,
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    m_encoderIndex(encoderIndex),
    m_refManager(parentRefManager),
    m_timeProvider(timeProvider),
    m_info(cameraInfo),
    m_codecParams(codecParams),
    m_ffmpegStreamReader(ffmpegStreamReader),
    m_interrupted(false)
{
    NX_ASSERT(m_timeProvider);

    m_consumer.reset(
        new ffmpeg::BufferedStreamConsumer(m_ffmpegStreamReader, codecParams));
}

StreamReader::~StreamReader()
{
    m_timeProvider->releaseRef();
    m_ffmpegStreamReader->removeConsumer(m_consumer);
}

void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

void StreamReader::interrupt()
{
    m_interrupted = true;
}

void StreamReader::setFps(int fps)
{
    m_codecParams.fps = fps;
    m_consumer->setFps(fps);
}

void StreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_codecParams.setResolution(resolution.width, resolution.height);
    m_consumer->setResolution(resolution.width, resolution.height);
}

void StreamReader::setBitrate(int bitrate)
{
    m_codecParams.bitrate = bitrate;
    m_consumer->setBitrate(bitrate);
}

void StreamReader::updateCameraInfo( const nxcip::CameraInfo& info )
{
    m_info = info;
}

int StreamReader::lastFfmpegError() const
{
    return m_lastFfmpegError;
}

std::unique_ptr<ILPVideoPacket> StreamReader::toNxPacket(
    AVPacket *packet, 
    AVCodecID codecID,
    uint64_t timeUsec) 
{
    int keyFrame = (packet->flags & AV_PKT_FLAG_KEY) ? nxcip::MediaDataPacket::fKeyPacket : 0;

    std::unique_ptr<ILPVideoPacket> nxVideoPacket(new ILPVideoPacket(
        &m_allocator,
        0,
        timeUsec,
        keyFrame,
        0 ) );

    nxVideoPacket->resizeBuffer(packet->size);
    if (nxVideoPacket->data())
        memcpy(nxVideoPacket->data(), packet->data, packet->size);

    nxVideoPacket->setCodecType(ffmpeg::utils::toNxCompressionType(codecID));

    return nxVideoPacket;
}

std::shared_ptr<ffmpeg::Packet> StreamReader::nextPacket()
{
    std::shared_ptr<ffmpeg::Packet> packet;
    while(!packet && !m_interrupted)
        packet = m_consumer->popFront();
    return packet;
}

void StreamReader::maybeDropPackets()
{
    int gopSize = m_ffmpegStreamReader->gopSize();
    if(gopSize && m_consumer->size() > gopSize)
    {
        int dropped = m_consumer->dropOldNonKeyPackets();
        NX_DEBUG(this) << "StreamReader " << m_encoderIndex << " dropping " << dropped << "packets.";
    }
}

} // namespace usb_cam
} // namespace nx
