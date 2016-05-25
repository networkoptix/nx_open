#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <memory>

#include <QtCore/QtGlobal>
#include <QtCore/QElapsedTimer>

#include <proxy_decoder.h>

#include <nx/utils/flag_config.h>
#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

class ProxyVideoDecoderFlagConfig: public nx::utils::FlagConfig
{
public:
    using nx::utils::FlagConfig::FlagConfig;

    NX_FLAG(0, disable, "Fully disable ProxyVideoDecoder: isCompatible() -> false.");
    NX_FLAG(1, largeOnly, "isCompatible() will allow only width > 640.");

    // Debug output.
    NX_FLAG(0, enableOutput, "");
    NX_FLAG(0, enableTime, "");
    NX_FLAG(0, enableFps, "");

    // Choosing impl.
    NX_FLAG(1, implDisplay, "decodeToDisplayQueue() => displayDecoded() in frame handle().");
    NX_FLAG(0, implRgb, "decodeToRgb() -> AlignedMemVideoBuffer, without OpenGL.");
    NX_FLAG(0, implYuvPlanar, "decodeToYuvPlanar() -> AlignedMemVideoBuffer => Qt Shader.");
    NX_FLAG(0, implYuvNative, "decodeToYuvNative() -> AlignedMemVideoBuffer => Plugin Shader.");
    NX_FLAG(0, implGl, "decodeYuvPlanar() => Planar YUV Shader.");

    // Display impl options.
    NX_FLAG(1, displayAsync, "Delay displaying up to beforeSynchronizing/frameSwapped.");
    NX_INT_PARAM(15, displayAsyncSleepMs, "Delay before displaying frame in displayAsync mode.");
    NX_FLAG(1, displayAsyncGlFinish, "Perform glFlush() and glFinish() in displayAsync mode.");

    // OpenGL impl options.
    NX_FLAG(0, stopOnGlErrors, "");
    NX_FLAG(0, outputGlCalls, "");
    NX_FLAG(0, useGlGuiRendering, "");
    NX_FLAG(1, useSharedGlContext, "");
};
extern ProxyVideoDecoderFlagConfig conf;

void debugShowFps(const char* tag);

/** Debug tool - implicitly unaligned pointer. */
uint8_t* debugUnalignPtr(void* data);

/**
 * @return Null if compressedVideoData is null.
 */
std::unique_ptr<ProxyDecoder::CompressedFrame> createUniqueCompressedFrame(
    const QnConstCompressedVideoDataPtr& compressedVideoData);

#define PRINT qWarning().nospace() << OUTPUT_PREFIX
#define OUTPUT if (!conf.enableOutput) {} else qWarning().nospace() << OUTPUT_PREFIX

long getTimeMs();
void logTimeMs(long oldTime, const char* message);
#define TIME_BEGIN(TAG) long TIME_##TAG = conf.enableTime ? getTimeMs() : 0
#define TIME_END(TAG) if(conf.enableTime) logTimeMs(TIME_##TAG, #TAG)

/**
 * Is a stub if !conf.enableTime.
 */
class DebugTimer
{
public:
    DebugTimer(const char* name);
    ~DebugTimer();
    void mark(const char* tag);
    void finish(const char* tag);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight);

class YuvBuffer;
typedef std::shared_ptr<YuvBuffer> YuvBufferPtr;
typedef std::shared_ptr<const YuvBuffer> ConstYuvBufferPtr;

class YuvBuffer
{
public:
    YuvBuffer(const QSize& frameSize)
    :
        m_frameSize(frameSize),
        m_yBuffer(qMallocAligned(m_frameSize.width() * m_frameSize.height(), kMediaAlignment)),
        m_uBuffer(qMallocAligned( m_frameSize.width() * m_frameSize.height() / 4, kMediaAlignment)),
        m_vBuffer(qMallocAligned(m_frameSize.width() * m_frameSize.height() / 4, kMediaAlignment))
    {
        assert(frameSize.width() % 2 == 0);
        assert(frameSize.height() % 2 == 0);
    }

    ~YuvBuffer()
    {
        qFreeAligned(m_yBuffer);
        qFreeAligned(m_uBuffer);
        qFreeAligned(m_vBuffer);
    }

    QSize frameSize() const
    {
        return m_frameSize;
    }

    int yLineSize() const
    {
        return m_frameSize.width();
    }

    int uVLineSize() const
    {
        return m_frameSize.width() / 2;
    }

    uint8_t* y() const
    {
        return static_cast<uint8_t*>(m_yBuffer);
    }

    uint8_t* u() const
    {
        return static_cast<uint8_t*>(m_uBuffer);
    }

    uint8_t* v() const
    {
        return static_cast<uint8_t*>(m_vBuffer);
    }

private:
    const QSize m_frameSize;

    void* m_yBuffer = nullptr;
    void* m_uBuffer = nullptr;
    void* m_vBuffer = nullptr;
};

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
