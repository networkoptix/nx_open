#include "stream_reader.h"

#include <nx/utils/url.h>
#include <nx/utils/log/log_main.h>

#include "ffmpeg/utils.h"
#include "camera/buffered_stream_consumer.h"
#include "native_stream_reader.h"
#include "transcode_stream_reader.h"
#include "nx/utils/app_info.h"

namespace nx {
namespace usb_cam {

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    m_refManager(parentRefManager)
{
    if (codecParams.codecID == AV_CODEC_ID_NONE) // needs transcoding to a supported codec
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

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    std::unique_ptr<StreamReaderPrivate> streamReader)
    :
    m_refManager(parentRefManager),
    m_streamReader(std::move(streamReader))
{
}

StreamReader::~StreamReader()
{
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

//////////////////////////////////// StreamReaderPrivate /////////////////////////////////////////


StreamReaderPrivate::StreamReaderPrivate(
    int encoderIndex,
    const CodecParameters &codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    m_encoderIndex(encoderIndex),
    m_codecParams(codecParams),
    m_camera(camera),
    m_audioConsumer(std::make_shared<BufferedPacketConsumer>()),
    m_consumerAdded(false),
    m_interrupted(false)
{
}

StreamReaderPrivate::~StreamReaderPrivate()
{
    m_camera->audioStreamReader()->removePacketConsumer(m_audioConsumer);
}

void StreamReaderPrivate::interrupt()
{
    m_camera->audioStreamReader()->removePacketConsumer(m_audioConsumer);
    m_audioConsumer->interrupt();
    m_audioConsumer->flush();
    m_interrupted = true;
    m_consumerAdded = false;
}

void StreamReaderPrivate::setFps(float fps)
{
    m_codecParams.fps = fps;
}

void StreamReaderPrivate::setResolution(const nxcip::Resolution& resolution)
{
    m_codecParams.setResolution(resolution.width, resolution.height);
}

void StreamReaderPrivate::setBitrate(int bitrate)
{
    m_codecParams.bitrate = bitrate;
}

void StreamReaderPrivate::ensureConsumerAdded()
{
    if (!m_consumerAdded)
    {
        m_camera->audioStreamReader()->addPacketConsumer(m_audioConsumer);
        m_consumerAdded = true;
    }
}

std::unique_ptr<ILPMediaPacket> StreamReaderPrivate::toNxPacket(
    AVPacket *packet,
    AVCodecID codecID,
    nxcip::DataPacketType mediaType,
    uint64_t timeUsec,
    bool forceKeyPacket)
{
    int keyPacket;
    if (forceKeyPacket)
        keyPacket = nxcip::MediaDataPacket::fKeyPacket;
    else
        keyPacket = (packet->flags & AV_PKT_FLAG_KEY) ? nxcip::MediaDataPacket::fKeyPacket : 0;

    std::unique_ptr<ILPMediaPacket> nxPacket(new ILPMediaPacket(
        &m_allocator,
        0,
        timeUsec,
        keyPacket,
        0));

    nxPacket->setCodecType(ffmpeg::utils::toNxCompressionType(codecID));
    nxPacket->setMediaType(mediaType);

    nxPacket->resizeBuffer(packet->size);
    if (nxPacket->data())
        memcpy(nxPacket->data(), packet->data, packet->size);

    return nxPacket;
}

} // namespace usb_cam
} // namespace nx
