#include "stream_reader.h"

#include <nx/utils/url.h>
#include <nx/utils/log/log.h>

#include "ffmpeg/utils.h"
#include "camera/buffered_stream_consumer.h"
#include "native_stream_reader.h"
#include "transcode_stream_reader.h"


namespace nx {
namespace usb_cam {

namespace  {

static constexpr int kUsecInMsec = 1000;

}

//-------------------------------------------------------------------------------------------------
// StreamReader

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera,
    bool forceTranscoding)
    :
    m_refManager(parentRefManager)
{
    // needs transcoding to a supported codec
    if (forceTranscoding)
        m_streamReader.reset(new TranscodeStreamReader(
        encoderIndex,
        codecParams,
        camera));
    else
        m_streamReader.reset(new NativeStreamReader(
        encoderIndex,
        codecParams,
        camera));
}

void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0)
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0)
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

int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    return m_streamReader->getNextData(lpPacket);
}

void StreamReader::interrupt()
{
    m_streamReader->interrupt();
}

void StreamReader::setFps(float fps)
{
    m_streamReader->setFps(fps);
}

void StreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_streamReader->setResolution(resolution);
}

void StreamReader::setBitrate(int bitrate)
{
    m_streamReader->setBitrate(bitrate);
}

//-------------------------------------------------------------------------------------------------
// StreamReaderPrivate

StreamReaderPrivate::StreamReaderPrivate(
    int encoderIndex,
    const CodecParameters &codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    m_encoderIndex(encoderIndex),
    m_codecParams(codecParams),
    m_camera(camera),
    m_avConsumer(new BufferedPacketConsumer(m_camera->videoStream(), m_codecParams))
{
}

StreamReaderPrivate::~StreamReaderPrivate()
{
    m_avConsumer->interrupt();
    removeAudioConsumer();
}

void StreamReaderPrivate::interrupt()
{
    m_avConsumer->interrupt();
    m_avConsumer->flush();

    m_interrupted = true;
}

void StreamReaderPrivate::setFps(float fps)
{
    m_codecParams.fps = fps;
}

void StreamReaderPrivate::setResolution(const nxcip::Resolution& resolution)
{
    m_codecParams.resolution = resolution;
}

void StreamReaderPrivate::setBitrate(int bitrate)
{
    m_codecParams.bitrate = bitrate;
}

void StreamReaderPrivate::ensureConsumerAdded()
{
    if (!m_audioConsumerAdded)
    {
        m_camera->audioStream()->addPacketConsumer(m_avConsumer);
        m_audioConsumerAdded = true;
    }
}

std::unique_ptr<ILPMediaPacket> StreamReaderPrivate::toNxPacket(const ffmpeg::Packet *packet)
{
    int keyPacket = packet->keyPacket() ? nxcip::MediaDataPacket::fKeyPacket : 0;

    std::unique_ptr<ILPMediaPacket> nxPacket(new ILPMediaPacket(
        &m_allocator,
        packet->mediaType() == AVMEDIA_TYPE_VIDEO ? 0 : 1,
        ffmpeg::utils::toNxDataPacketType(packet->mediaType()),
        ffmpeg::utils::toNxCompressionType(packet->codecId()),
        packet->timestamp() * kUsecInMsec,
        keyPacket,
        0));

    nxPacket->resizeBuffer(packet->size());
    if (nxPacket->data())
        memcpy(nxPacket->data(), packet->data(), packet->size());

    return nxPacket;
}

void StreamReaderPrivate::removeAudioConsumer()
{
    m_camera->audioStream()->removePacketConsumer(m_avConsumer);
    m_audioConsumerAdded = false;
}

void StreamReaderPrivate::removeConsumer()
{
    removeVideoConsumer();
    removeAudioConsumer();
}

} // namespace usb_cam
} // namespace nx
