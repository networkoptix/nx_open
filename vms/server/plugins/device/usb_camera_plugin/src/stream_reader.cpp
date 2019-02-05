#include "stream_reader.h"

#include <nx/utils/url.h>
#include <nx/utils/log/log.h>

#include "ffmpeg/utils.h"
#include "camera/buffered_stream_consumer.h"
#include "native_stream_reader.h"
#include "transcode_stream_reader.h"


namespace nx::usb_cam {

namespace  {

static constexpr int kUsecInMsec = 1000;

}

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    const std::shared_ptr<Camera>& camera,
    bool forceTranscoding)
    :
    m_refManager(parentRefManager),
    m_camera(camera),
    m_avConsumer(new BufferedPacketConsumer(camera->toString()))
{
    if (forceTranscoding)
        m_streamReader.reset(new TranscodeStreamReader(camera));
    else
        m_streamReader.reset(new NativeStreamReader(camera));
}

StreamReader::~StreamReader()
{
    m_avConsumer->interrupt();
    removeConsumer();
}

void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if (memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader)) == 0)
    {
        addRef();
        return this;
    }
    if (memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

int StreamReader::addRef() const
{
    return m_refManager.addRef();
}

int StreamReader::releaseRef() const
{
    return m_refManager.releaseRef();
}

int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();

    auto packet = nextPacket();
    if (!packet)
        return handleNxError();

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

std::shared_ptr<ffmpeg::Packet> StreamReader::nextPacket()
{
    while (!shouldStop())
    {
        auto popped =  m_avConsumer->pop();
        if (!popped)
            continue;

        std::shared_ptr<ffmpeg::Packet> result;
        int status = m_streamReader->processPacket(popped, result);
        if (status < 0)
        {
            m_camera->setLastError(status);
            return nullptr;
        }

        if (!result)
            continue;

        return result;
    }
    return nullptr;
}

void StreamReader::interrupt()
{
    m_avConsumer->interrupt();
    m_avConsumer->flush();

    m_interrupted = true;
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

void StreamReader::ensureConsumerAdded()
{
    if (!m_audioConsumerAdded)
    {
        m_camera->audioStream()->addPacketConsumer(m_avConsumer);
        m_audioConsumerAdded = true;
    }

    if (!m_videoConsumerAdded)
    {
        m_camera->videoStream()->addPacketConsumer(m_avConsumer);
        m_videoConsumerAdded = true;
    }
}

std::unique_ptr<ILPMediaPacket> StreamReader::toNxPacket(const ffmpeg::Packet *packet)
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

void StreamReader::removeConsumer()
{
    m_camera->audioStream()->removePacketConsumer(m_avConsumer);
    m_audioConsumerAdded = false;

    m_camera->videoStream()->removePacketConsumer(m_avConsumer);
    m_videoConsumerAdded = false;
}

bool StreamReader::interrupted()
{
    if (m_interrupted)
    {
        m_interrupted = false;
        return true;
    }
    return false;
}

int StreamReader::handleNxError()
{
    removeConsumer();

    if (interrupted())
        return nxcip::NX_INTERRUPTED;


    if (m_camera->ioError())
        return nxcip::NX_IO_ERROR;

    return nxcip::NX_OTHER_ERROR;
}

bool StreamReader::shouldStop() const
{
    return m_interrupted || m_camera->ioError();
}

} // namespace usb_cam::nx
