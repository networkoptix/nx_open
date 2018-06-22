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

namespace nx {
namespace webcam_plugin {

NativeStreamReader::NativeStreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    const CodecContext& codecContext,
    const std::weak_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    StreamReader(
        parentRefManager,
        timeProvider,
        cameraInfo,
        codecContext,
        ffmpegStreamReader)
{
    debug("%s\n", __FUNCTION__);
}

NativeStreamReader::~NativeStreamReader()
{
}

int NativeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    QnMutexLocker lock(&m_mutex);

    std::shared_ptr<ffmpeg::StreamReader> streamReader = m_ffmpegStreamReader.lock();
    if(!streamReader)
        return nxcip::NX_OTHER_ERROR;

    int loadCode = streamReader->loadNextData();
    AVPacket * packet = streamReader->currentPacket();

    auto nxPacket = toNxPacket(packet, streamReader->codec()->codecID());
    *lpPacket = nxPacket.release();

    return nxcip::NX_NO_ERROR;
}

void NativeStreamReader::setFps(int fps)
{
    if (auto streamReader = m_ffmpegStreamReader.lock())
        streamReader->setFps(fps);
}

void NativeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (auto streamReader = m_ffmpegStreamReader.lock())
        streamReader->setResolution(resolution.width, resolution.height);
}

void NativeStreamReader::setBitrate(int bitrate)
{
    if (auto streamReader = m_ffmpegStreamReader.lock())
        streamReader->setBitrate(bitrate);
}


} // namespace webcam_plugin
} // namespace nx
