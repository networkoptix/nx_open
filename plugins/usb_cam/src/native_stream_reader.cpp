#include "native_stream_reader.h"

#include <memory>

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
    std::cout << "NativeStreamReader" << std::endl;
}

NativeStreamReader::~NativeStreamReader()
{
    std::cout << "~NativeStreamReader" << std::endl;
    interrupt();
}

uint64_t now = 0;
uint64_t earlier = 0;
uint64_t lastTs = 0;

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();
    maybeDropPackets();
    
    auto packet = nextPacket();
    if (m_interrupted)
    {
        m_interrupted = false;
        return nxcip::NX_INTERRUPTED;
    }

    if(!packet)
        return nxcip::NX_OTHER_ERROR;

   /*now = m_camera->millisSinceEpoch();
    std::stringstream ss;
    ss << now - earlier
        << ", " << packet->timeStamp() - lastTs
        << ", " << ffmpeg::utils::codecIDToName(packet->codecID());
    std::cout << ss.str() << std::endl;
    earlier = now;
    lastTs = packet->timeStamp();*/

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::interrupt()
{
    StreamReaderPrivate::interrupt();
    m_camera->videoStream()->removePacketConsumer(m_avConsumer);
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

void NativeStreamReader::maybeDropPackets()
{
    if (m_avConsumer->timeSpan() > 1000)
        m_avConsumer->flush();
}

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket()
{
    if (m_camera->audioEnabled())
    {
        //static constexpr uint64_t bufferSizeMsec = 500;
        //if (!m_avConsumer->waitForTimeStampDifference(bufferSizeMsec))
        if (!m_avConsumer->wait(11))
            return nullptr;
    }
   
    return m_avConsumer->popFront();
}

} // namespace usb_cam
} // namespace nx