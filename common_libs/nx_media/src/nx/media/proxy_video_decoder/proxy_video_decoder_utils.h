#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <memory>
#include <deque>
#include <iostream>

#include <QtCore/QtGlobal>

#include <nx/kit/ini_config.h>

#include <nx/utils/log/log.h>
#include <nx/streaming/video_data_packet.h>

#include <nx/media/media_fwd.h>

#include <proxy_decoder.h>

namespace nx {
namespace media {

namespace proxy_video_decoder {

class Ini: public nx::kit::IniConfig
{
public:
    Ini(): IniConfig("ProxyVideoDecoder.ini") {}

    NX_INI_FLAG(0, disable, "Fully disable ProxyVideoDecoder: isCompatible() -> false.");
    NX_INI_FLAG(1, largeOnly, "isCompatible() will allow only width > 640.");

    // Debug output.
    NX_INI_FLAG(0, enableOutput, "");
    NX_INI_FLAG(0, enableTime, "");
    NX_INI_FLAG(0, enableFpsHandle, "Measure FPS of VideoBuffer.handle() calls.");

    // Choosing impl.
    NX_INI_FLAG(0, implStub, "Checkerboard stub, do not use libproxydecoder.");
    NX_INI_FLAG(1, implDisplay, "decodeToDisplayQueue() => displayDecoded() in frame handle().");
    NX_INI_FLAG(0, implRgb, "decodeToRgb() -> AlignedMemVideoBuffer, without OpenGL.");
    NX_INI_FLAG(0, implYuvPlanar, "decodeToYuvPlanar() -> AlignedMemVideoBuffer => Qt Shader.");
    NX_INI_FLAG(0, implYuvNative, "decodeToYuvNative() -> AlignedMemVideoBuffer => Plugin Shader.");
    NX_INI_FLAG(0, implGl, "decodeYuvPlanar() => Planar YUV Shader.");

    // Display impl options.
    NX_INI_FLAG(0, displayAsync, "Delay displaying up to beforeSynchronizing/frameSwapped.");
    NX_INI_INT(15, displayAsyncSleepMs, "Delay before displaying frame in displayAsync mode.");
    NX_INI_FLAG(1, displayAsyncGlFinish, "Perform glFlush() and glFinish() in displayAsync mode.");

    // OpenGL impl options.
    NX_INI_FLAG(0, stopOnGlErrors, "");
    NX_INI_FLAG(0, outputGlCalls, "");
    NX_INI_FLAG(0, useGlGuiRendering, "");
    NX_INI_FLAG(1, useSharedGlContext, "");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace proxy_video_decoder

using proxy_video_decoder::ini;

// For debug.
std::ostream& operator<<(std::ostream& stream, const QSize& qSize);
std::ostream& operator<<(std::ostream& stream, const QString& s);

/**
 * @return Null if compressedVideoData is null.
 */
std::unique_ptr<ProxyDecoder::CompressedFrame> createUniqueCompressedFrame(
    const QnConstCompressedVideoDataPtr& compressedVideoData);

void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight);

class YuvBuffer;
typedef std::shared_ptr<YuvBuffer> YuvBufferPtr;
typedef std::shared_ptr<const YuvBuffer> ConstYuvBufferPtr;

class YuvBuffer
{
public:
    YuvBuffer(const QSize& frameSize):
        m_frameSize(frameSize),
        m_yBuffer(qMallocAligned(m_frameSize.width() * m_frameSize.height(), kMediaAlignment)),
        m_uBuffer(qMallocAligned(m_frameSize.width() * m_frameSize.height() / 4, kMediaAlignment)),
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

#endif // defined(ENABLE_PROXY_DECODER)
