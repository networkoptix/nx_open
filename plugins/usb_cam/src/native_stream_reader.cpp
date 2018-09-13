#include "native_stream_reader.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "utils.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

namespace {

uint64_t kMsecInSec = 1000;

uint64_t now = 0;
uint64_t earlier = 0;


}

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
    interrupt();
}

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

    now = m_camera->millisSinceEpoch();
    std::stringstream ss;
    ss << packet->timeStamp()
        << ", " << now - earlier
        << ", " << packet->timeStamp() - m_lastTs
        << ", " << ffmpeg::utils::codecIDToName(packet->codecID());
    //std::cout << ss.str() << std::endl;
    earlier = now;

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
    if (m_avConsumer->timeSpan() > kMsecInSec)
        m_avConsumer->flush();
}

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket()
{
    if (m_camera->audioEnabled())
    {
        uint64_t msecPerFrame = utils::msecPerFrame(m_camera->videoStream()->fps());
        if( !m_avConsumer->waitForTimeSpan(msecPerFrame))
            return nullptr;
    }
   
    return m_avConsumer->popFront();
}

} // namespace usb_cam
} // namespace nx