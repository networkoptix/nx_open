#include "native_stream_reader.h"

#include <nx/utils/log/log.h>

#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

NativeStreamReader::NativeStreamReader(int encoderIndex, const std::shared_ptr<Camera>& camera):
    StreamReaderPrivate(encoderIndex, camera)
{
    CodecParameters codecParams = camera->defaultVideoParameters();
    m_camera->videoStream()->setResolution(codecParams.resolution);
    m_camera->videoStream()->setFps(codecParams.fps);
    m_camera->videoStream()->setBitrate(codecParams.bitrate);
}

NativeStreamReader::~NativeStreamReader()
{
    m_avConsumer->interrupt();
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();

    auto packet = nextPacket();

    if (!packet)
        return handleNxError();

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

std::shared_ptr<ffmpeg::Packet> NativeStreamReader::nextPacket()
{
    while (!shouldStop())
    {
        auto popped = m_avConsumer->pop();
        if (!popped)
            continue;

        return popped;
    }
    return nullptr;
}

} // namespace usb_cam
} // namespace nx
