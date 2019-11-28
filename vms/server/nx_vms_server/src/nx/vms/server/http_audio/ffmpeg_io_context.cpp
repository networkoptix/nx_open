#include "ffmpeg_io_context.h"

extern "C" {

#include <libavutil/error.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavformat/avio.h>

} // extern "C"

namespace nx::vms::server::http_audio {

static int ffmpegRead(void *opaque, uint8_t* buffer, int size);
static int ffmpegWrite(void *opaque, uint8_t* buf, int size);
static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence);

FfmpegIoContext::FfmpegIoContext(uint32_t bufferSize, bool writable)
{
    m_buffer = (uint8_t*) av_malloc(bufferSize);
    m_avioContext = avio_alloc_context(
        m_buffer,
        bufferSize,
        writable ? 1 : 0,
        this,
        &ffmpegRead,
        &ffmpegWrite,
        &ffmpegSeek);
}

FfmpegIoContext::~FfmpegIoContext()
{
    avio_flush(m_avioContext);
    m_avioContext->opaque = 0;
    av_freep(&m_avioContext->buffer);
    av_opt_free(m_avioContext);
    av_free(m_avioContext);
}

AVIOContext* FfmpegIoContext::getAvioContext()
{
    return m_avioContext;
}

static int ffmpegRead(void *opaque, uint8_t* buffer, int size)
{
    FfmpegIoContext* owner = reinterpret_cast<FfmpegIoContext*>(opaque);
    if (!owner->readHandler)
        return -1;
    return owner->readHandler(buffer, size);
}

static int ffmpegWrite(void *opaque, uint8_t* buffer, int size)
{
    FfmpegIoContext* owner = reinterpret_cast<FfmpegIoContext*>(opaque);
    if (!owner->writeHandler)
        return -1;
    return owner->writeHandler(buffer, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    FfmpegIoContext* owner = reinterpret_cast<FfmpegIoContext*>(opaque);
    if (!owner->seekHandler)
        return -1;
    return owner->seekHandler(pos, whence);
}

} // namespace nx::vms::server::http_audio
