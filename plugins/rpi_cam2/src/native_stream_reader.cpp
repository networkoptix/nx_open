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
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec.h"
#include "ffmpeg/utils.h"
#include "ffmpeg/error.h"
#include "ffmpeg/packet.h"

namespace nx {
namespace rpi_cam2 {

NativeStreamReader::NativeStreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const CodecContext& codecContext,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecContext,
        ffmpegStreamReader)
{
}

NativeStreamReader::~NativeStreamReader()
{
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    if(nx::ffmpeg::error::hasError())
        debug("ffmpegError: %s\n", nx::ffmpeg::error::avStrError(nx::ffmpeg::error::lastError()).c_str());

    int loadCode = m_ffmpegStreamReader->loadNextData();
    if(loadCode < 0)
        return nxcip::NX_IO_ERROR;

    auto nxPacket = toNxPacket(m_ffmpegStreamReader->currentPacket()->packet(),
        m_ffmpegStreamReader->decoderID());
    *lpPacket = nxPacket.release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::setFps(int fps)
{
    m_ffmpegStreamReader->setFps(fps);
}

void NativeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    m_ffmpegStreamReader->setResolution(resolution.width, resolution.height);
}

void NativeStreamReader::setBitrate(int bitrate)
{
    m_ffmpegStreamReader->setBitrate(bitrate);
}


} // namespace rpi_cam2
} // namespace nx
