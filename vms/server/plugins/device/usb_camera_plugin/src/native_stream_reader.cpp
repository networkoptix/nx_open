#include "native_stream_reader.h"

#include <nx/utils/log/log.h>

#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

NativeStreamReader::NativeStreamReader(
    int encoderIndex,
    const CodecParameters& codecParams,
    const std::shared_ptr<Camera>& camera)
    :
    StreamReaderPrivate(encoderIndex, camera)
{
    m_camera->videoStream()->setResolution(codecParams.resolution);
    m_camera->videoStream()->setFps(codecParams.fps);
    m_camera->videoStream()->setBitrate(codecParams.bitrate);
}

NativeStreamReader::~NativeStreamReader()
{
    m_avConsumer->interrupt();
    // Avoid virtual removeVideoConsumer()
    m_camera->videoStream()->removePacketConsumer(m_avConsumer);
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();

    auto packet = nextPacket();

    if (!packet)
    {
        removeConsumer();
        if (m_interrupted)
        {
            m_interrupted = false;
            return nxcip::NX_INTERRUPTED;
        }

        if (m_camera->ioError())
            return nxcip::NX_IO_ERROR;

        return nxcip::NX_OTHER_ERROR;
    }

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::setFps(float fps)
{
    m_camera->videoStream()->setFps(fps);
}
void NativeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_camera->videoStream()->setResolution(resolution);
}

void NativeStreamReader::setBitrate(int bitrate)
{
    m_camera->videoStream()->setBitrate(bitrate);
}

void NativeStreamReader::ensureConsumerAdded()
{
    if (!m_audioConsumerAdded)
        StreamReaderPrivate::ensureConsumerAdded();

    if(! m_videoConsumerAdded)
    {
        m_camera->videoStream()->addPacketConsumer(m_avConsumer);
        m_videoConsumerAdded = true;
    }
}

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket()
{
    while (m_camera->audioEnabled())
    {
        // the time span keeps audio and video timestamps monotonic
        if (m_avConsumer->waitForTimespan(kStreamDelay, kWaitTimeout))
            break;
        else if (m_interrupted || m_camera->ioError())
                return nullptr;
    }

    for (;;)
    {
        auto popped = m_avConsumer->popOldest(kWaitTimeout);
        if (!popped)
        {
            if (m_interrupted || m_camera->ioError())
                return nullptr;
            continue;
        }
        return popped;
    }
}

void NativeStreamReader::removeVideoConsumer()
{
    m_camera->videoStream()->removePacketConsumer(m_avConsumer);
    m_videoConsumerAdded = false;
}

} // namespace usb_cam
} // namespace nx
