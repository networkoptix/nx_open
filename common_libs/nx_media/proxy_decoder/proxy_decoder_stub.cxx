// Alternative impl of ProxyDecoder as a pure software stub which just draws a checkerboard.
// Intended to be included into "proxydecoder.cpp".

#include <cstdint>
#include <cassert>
#include <cstring>
#include <unistd.h>

#define LOG_PREFIX "ProxyDecoder[STUB]"
#include "proxy_decoder_debug.cxx"

/**
 * Value object.
 */
class ProxyDecoderPrivate
{
public:
    ProxyDecoderPrivate()
    {
        memset(this, 0, sizeof(this));
    }

    int frameWidth;
    int frameHeight;

    uint8_t* nativeYuvBuffer;
    int nativeYuvBufferSize;
};

ProxyDecoder::ProxyDecoder(int frameWidth, int frameHeight)
:
    d(new ProxyDecoderPrivate())
{
    assert(frameWidth > 0);
    assert(frameHeight > 0);

    d->frameWidth = frameWidth;
    d->frameHeight = frameHeight;

    LOG(":ProxyDecoder(frameWidth: " << frameWidth
        << ", frameHeight: " << frameHeight << ")");

    // Native buffer is NV12 (12 bits per pixel), arranged in 32x32 px macroblocks.
    d->nativeYuvBufferSize =
        ((frameWidth + 15) / 16 * 16) *
        ((frameHeight + 15) / 16 * 16) * 3 / 2;

    d->nativeYuvBuffer = (uint8_t*) malloc(d->nativeYuvBufferSize);
    memset(d->nativeYuvBuffer, 0, d->nativeYuvBufferSize);
}

ProxyDecoder::~ProxyDecoder()
{
    LOG(":~ProxyDecoder() BEGIN");
    free(d->nativeYuvBuffer);
    LOG(":~ProxyDecoder() END");
}

int ProxyDecoder::decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    assert(argbBuffer);
    assert(argbLineSize > 0);

    // Log if parameters change.
    static int prevArgbLineSize = -1;
    if (prevArgbLineSize != argbLineSize)
    {
        prevArgbLineSize = argbLineSize;
        LOG(":decodeToRgb(argbLineSize: " << argbLineSize << ")");
    }

    static int frameNumber = 0;

    memset(argbBuffer, 0, argbLineSize * d->frameHeight);
    debugDrawCheckerboardArgb(argbBuffer, argbLineSize, d->frameWidth, d->frameHeight);

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++frameNumber;
}

int ProxyDecoder::decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    assert(yBuffer);
    assert(uBuffer);
    assert(vBuffer);
    assert(yLineSize > 0);
    assert(uVLineSize > 0);

    // Log if parameters change.
    static int prevYLineSize = -1;
    static int prevUVLineSize = -1;
    if (prevYLineSize != yLineSize || prevUVLineSize != uVLineSize)
    {
        prevYLineSize = yLineSize;
        prevUVLineSize = uVLineSize;
        LOG(":decodeToYuvPlanar(yLineSize: " << yLineSize
            << ", uVLineSize: " << uVLineSize << ")");
    }

    static int frameNumber = 0;

    memset(yBuffer, 0, yLineSize * d->frameHeight);
    debugDrawCheckerboardY(yBuffer, yLineSize, d->frameWidth, d->frameHeight);

    memset(uBuffer, 0, uVLineSize * (d->frameHeight / 2));
    memset(vBuffer, 0, uVLineSize * (d->frameHeight / 2));

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++frameNumber;
}

int ProxyDecoder::decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t** outBuffer, int* outBufferSize)
{
    static int frameNumber = 0;

    assert(outBuffer);
    assert(outBufferSize);

    // TODO: Draw something visible in the "native" buffer.

    *outBuffer = d->nativeYuvBuffer;
    *outBufferSize = d->nativeYuvBufferSize;

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++frameNumber;
}
