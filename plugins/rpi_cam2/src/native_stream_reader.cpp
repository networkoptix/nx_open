#include "native_stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#undef max
#undef min
#else
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <errno.h>
#endif
#endif

#include <sys/timeb.h>
#include <memory>
#include <chrono>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "utils/utils.h"
#include "device/utils.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/error.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace rpi_cam2 {

namespace 
{
    int lastErrFfmpeg = 0;

    void logError(const NativeStreamReader * logObject)
    {
        if (int err = ffmpeg::error::lastError())
        {
            if (err != lastErrFfmpeg)
            {
                lastErrFfmpeg = err;
                std::string errStr = ffmpeg::error::toString(err);
                debug("Last ffmpeg error: %s\n", errStr.c_str());
                NX_DEBUG(logObject) << "Last ffmpeg error: " << errStr;
            }
        }
    }
}

NativeStreamReader::NativeStreamReader(
    int encoderIndex,
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const CodecContext& codecContext,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        encoderIndex,
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecContext,
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

    logError(this);

    if (!m_initialized)
    {
        m_ffmpegStreamReader->addConsumer(m_consumer);
        m_initialized = true;
    }

    maybeDropPackets();
    auto packet = nextPacket();

    *lpPacket = toNxPacket(
        packet->packet(),
        packet->codecID(),
        packet->timeStamp() * 1000).release();

    return nxcip::NX_NO_ERROR;
}

} // namespace rpi_cam2
} // namespace nx
