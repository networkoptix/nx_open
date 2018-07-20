#include "native_stream_reader.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "utils/utils.h"
#include "device/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/error.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace rpi_cam2 {

NativeStreamReader::NativeStreamReader(
    int encoderIndex,
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const ffmpeg::CodecParameters& codecParams,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        encoderIndex,
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecParams,
        ffmpegStreamReader),
    m_initialized(false)
{
}

NativeStreamReader::~NativeStreamReader()
{
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if (!m_initialized)
    {
        m_ffmpegStreamReader->addConsumer(m_consumer);
        m_initialized = true;
    }
    
    maybeDropPackets();
    auto packet = nextPacket();

    if(m_interrupted)
    {
        m_interrupted = false;
        return nxcip::NX_NO_DATA;
    }

    *lpPacket = toNxPacket(
        packet->packet(),
        packet->codecID(),
        packet->timeStamp() * 1000).release();

    return nxcip::NX_NO_ERROR;
}

} // namespace rpi_cam2
} // namespace nx
