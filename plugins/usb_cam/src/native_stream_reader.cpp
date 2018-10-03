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

static constexpr int kMsecInSec = 1000;
static constexpr uint64_t kStreamDelayMsec = 100;

} // namespace

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
    m_camera->videoStream()->removePacketConsumer(m_avConsumer);
    m_avConsumer->interrupt();
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();
    maybeFlush();
    
    auto packet = nextPacket();
    if (m_interrupted)
    {
        m_interrupted = false;
        return nxcip::NX_INTERRUPTED;
    }

    if(!packet)
        return nxcip::NX_OTHER_ERROR;

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

void NativeStreamReader::maybeFlush()
{
    if (m_avConsumer->timeSpan() > kMsecInSec)
        m_avConsumer->flush();
}

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket()
{
    if (m_camera->audioEnabled())
    {
        if (!m_avConsumer->waitForTimeSpan(kStreamDelayMsec))
            return nullptr;
    }
   
    return m_avConsumer->popFront();
}

} // namespace usb_cam
} // namespace nx