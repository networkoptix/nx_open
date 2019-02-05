#include "native_stream_reader.h"

#include <nx/utils/log/log.h>

#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace usb_cam {

NativeStreamReader::NativeStreamReader(const std::shared_ptr<Camera>& camera):
    m_camera(camera)
{
    CodecParameters codecParams = camera->defaultVideoParameters();
    camera->videoStream()->setResolution(codecParams.resolution);
    camera->videoStream()->setFps(codecParams.fps);
    camera->videoStream()->setBitrate(codecParams.bitrate);
}

int NativeStreamReader::processPacket(
    const std::shared_ptr<ffmpeg::Packet>& source, std::shared_ptr<ffmpeg::Packet>& result)
{
    result = source;
    return 0;
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

} // namespace usb_cam
} // namespace nx
