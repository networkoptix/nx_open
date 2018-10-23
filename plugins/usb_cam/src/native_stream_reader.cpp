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
    StreamReaderPrivate(
        encoderIndex,
        codecParams,
        camera)
{
}

NativeStreamReader::~NativeStreamReader()
{
    m_avConsumer->interrupt();
    m_camera->videoStream()->removePacketConsumer(m_avConsumer);  //< Avoid virtual removeConsumer()
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();
    maybeFlush();

    auto packet = nextPacket();

    if(!packet)
    {
        removeConsumer();

        if (m_interrupted)
        {
            m_interrupted = false;
            return nxcip::NX_INTERRUPTED;
        }

        if(m_camera->videoStream()->ioError())
            return nxcip::NX_IO_ERROR;
        
        return nxcip::NX_OTHER_ERROR;
    }

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::interrupt()
{
    StreamReaderPrivate::interrupt();
}

void NativeStreamReader::setFps(float fps)
{
    StreamReaderPrivate::setFps(fps);
    m_avConsumer->setFps(fps);
}
void NativeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    StreamReaderPrivate::setResolution(resolution);
    m_avConsumer->setResolution(resolution.width, resolution.height);
}

void NativeStreamReader::setBitrate(int bitrate)
{
    StreamReaderPrivate::setBitrate(bitrate);
    m_avConsumer->setBitrate(bitrate);
}

void NativeStreamReader::ensureConsumerAdded()
{
    if (!m_consumerAdded)
    {
        StreamReaderPrivate::ensureConsumerAdded();
        m_camera->videoStream()->addPacketConsumer(m_avConsumer);
    }
}

void NativeStreamReader::maybeFlush()
{
    if (m_avConsumer->timeSpan() > kBufferTimeSpanMax)
        m_avConsumer->flush();
}

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket()
{
    if (m_camera->audioEnabled())
    {
        for(;;)
        {
            // the time span keeps audio and video timestamps monotonic
            if (m_avConsumer->waitForTimeSpan(kStreamDelay, kWaitTimeOut))
                break;
            else if (m_interrupted || m_camera->videoStream()->ioError())
                    return nullptr;
        }            
    }

    for(;;)
    {
        auto popped = m_avConsumer->popOldest(kWaitTimeOut);
        if (!popped)
        {
            if (m_interrupted || m_camera->videoStream()->ioError())
                return nullptr;
            continue;
        }
        return popped;
    }
}

void NativeStreamReader::removeConsumer()
{
    StreamReaderPrivate::removeConsumer();
    m_camera->videoStream()->removePacketConsumer(m_avConsumer);
}

} // namespace usb_cam
} // namespace nx