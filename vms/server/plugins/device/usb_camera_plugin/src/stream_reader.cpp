#include "stream_reader.h"

#include <nx/utils/url.h>
#include <nx/utils/log/log.h>

#include "ffmpeg/utils.h"

namespace nx::usb_cam {

namespace  {

static constexpr int kUsecInMsec = 1000;

}

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    const std::shared_ptr<Camera>& camera,
    bool isPrimaryStream)
    :
    m_refManager(parentRefManager),
    m_camera(camera),
    m_isPrimaryStream(isPrimaryStream)
{
    if (!m_isPrimaryStream || !camera->videoStream().isVideoCompressed())
        m_transcodeReader.reset(new TranscodeStreamReader(camera));
}

void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if (memcmp(&interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader)) == 0)
    {
        addRef();
        return this;
    }
    if (memcmp(&interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface)) == 0)
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
    m_interrupted = false;
    if (!m_isPrimaryStream)
        m_camera->enablePacketBuffering(true);
    *lpPacket = nullptr;

    std::shared_ptr<ffmpeg::Packet> packet;
    int nxStatus = nextPacket(packet);
    if (nxStatus != nxcip::NX_NO_ERROR)
        return nxStatus;

    *lpPacket = toNxPacket(packet.get()).release();
    return nxcip::NX_NO_ERROR;
}

int StreamReader::nextPacket(std::shared_ptr<ffmpeg::Packet>& packet)
{
    while (!m_interrupted)
    {
        int status;
        if (m_isPrimaryStream)
            status = m_camera->nextPacket(packet);
        else
            status = m_camera->nextBufferedPacket(packet);
        if (status == AVERROR(EAGAIN))
            continue;
        if (status < 0)
        {
            NX_ERROR(this, "Usb camera plugin reading error: %1",
                ffmpeg::utils::errorToString(status));
            m_camera->uninitialize();
            return nxcip::NX_IO_ERROR;
        }
        if (!packet)
            continue;

        if (packet->mediaType() == AVMEDIA_TYPE_AUDIO)
            return nxcip::NX_NO_ERROR;

        if (m_transcodeReader)
        {
            std::shared_ptr<ffmpeg::Packet> transcoded;
            status = m_transcodeReader->transcode(packet, transcoded);
            if (status == AVERROR(EAGAIN))
                continue;

            if (status < 0)
            {
                NX_ERROR(this, "Usb camera plugin transcoding error: %1",
                    ffmpeg::utils::errorToString(status));
                return nxcip::NX_OTHER_ERROR;
            }
            packet = transcoded;
        }

        return nxcip::NX_NO_ERROR;
    }
    return nxcip::NX_INTERRUPTED;
}

void StreamReader::interrupt()
{
    m_interrupted = true;
    if (m_isPrimaryStream)
        m_camera->uninitialize();
    else
    {
        m_camera->interruptBufferPacketWait();
        m_camera->enablePacketBuffering(false);
    }
}

void StreamReader::setFps(float fps)
{
    if (m_isPrimaryStream)
        m_camera->videoStream().setFps(fps);
    if (m_transcodeReader)
        m_transcodeReader->setFps(fps);
}

void StreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_isPrimaryStream)
        m_camera->videoStream().setResolution(resolution);
    if (m_transcodeReader)
        m_transcodeReader->setResolution(resolution);
}

void StreamReader::setBitrate(int bitrate)
{
    if (m_isPrimaryStream)
        m_camera->videoStream().setBitrate(bitrate);
    if (m_transcodeReader)
        m_transcodeReader->setBitrate(bitrate);
}

std::unique_ptr<ILPMediaPacket> StreamReader::toNxPacket(const ffmpeg::Packet *packet)
{
    std::unique_ptr<ILPMediaPacket> nxPacket(new ILPMediaPacket(
        &m_allocator,
        packet->mediaType() == AVMEDIA_TYPE_VIDEO ? 0 : 1,
        ffmpeg::utils::toNxDataPacketType(packet->mediaType()),
        ffmpeg::utils::toNxCompressionType(packet->codecId()),
        packet->timestamp() * kUsecInMsec,
        packet->keyPacket() ? nxcip::MediaDataPacket::fKeyPacket : 0,
        0));

    nxPacket->resizeBuffer(packet->size());
    if (nxPacket->data())
        memcpy(nxPacket->data(), packet->data(), packet->size());

    return nxPacket;
}

} // namespace usb_cam::nx
