#include "proxy_decoder.h"

// Alternative impl of ProxyDecoder as a pure software stub which just draws a checkerboard.

#include <cassert>
#include <cstring>

#define LOG_PREFIX "ProxyDecoder[STUB]: "
#include "proxy_decoder_utils.h"

namespace {

class Stub
:
    public ProxyDecoder
{
public:
    Stub(int frameWidth, int frameHeight);
    virtual ~Stub() override;

    virtual int decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* argbBuffer, int argbLineSize) override;

    virtual int decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize) override;

    virtual int decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t** outBuffer, int* outBufferSize) override;

    virtual int decodeToDisplayQueue(
        const CompressedFrame* compressedFrame, int64_t* outPts,
        void **outFrameHandle) override;

    virtual void displayDecoded(void* frameHandle) override;

private:
    int m_frameWidth;
    int m_frameHeight;

    uint8_t* m_nativeYuvBuffer = nullptr;
    int m_nativeYuvBufferSize = 0;

    int m_frameNumber;
};

Stub::Stub(int frameWidth, int frameHeight)
:
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight),
    m_frameNumber(0)
{
    assert(frameWidth > 0);
    assert(frameHeight > 0);

    LOG << "Stub(frameWidth: " << frameWidth << ", frameHeight: " << frameHeight << ")";

    // Native buffer is NV12 (12 bits per pixel), arranged in 32x32 px macroblocks.
    m_nativeYuvBufferSize =
        ((frameWidth + 15) / 16 * 16) *
        ((frameHeight + 15) / 16 * 16) * 3 / 2;

    m_nativeYuvBuffer = (uint8_t*) malloc(m_nativeYuvBufferSize);
    memset(m_nativeYuvBuffer, 0, m_nativeYuvBufferSize);
}

Stub::~Stub()
{
    LOG << "~Stub() BEGIN";
    free(m_nativeYuvBuffer);
    LOG << "~Stub() END";
}

int Stub::decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    assert(argbBuffer);
    assert(argbLineSize > 0);

    // Log if parameters change.
    static int prevArgbLineSize = -1;
    if (prevArgbLineSize != argbLineSize)
    {
        prevArgbLineSize = argbLineSize;
        LOG << "decodeToRgb(argbLineSize: " << argbLineSize << ")";
    }

    static int frameNumber = 0;

    memset(argbBuffer, 0, argbLineSize * m_frameHeight);
    debugDrawCheckerboardArgb(argbBuffer, argbLineSize, m_frameWidth, m_frameHeight);

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++frameNumber;
}

int Stub::decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPts,
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
        LOG << "decodeToYuvPlanar(yLineSize: " << yLineSize
            << ", uVLineSize: " << uVLineSize << ")";
    }

    memset(yBuffer, 0, yLineSize * m_frameHeight);
    debugDrawCheckerboardY(yBuffer, yLineSize, m_frameWidth, m_frameHeight);

    memset(uBuffer, 0, uVLineSize * (m_frameHeight / 2));
    memset(vBuffer, 0, uVLineSize * (m_frameHeight / 2));

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++m_frameNumber;
}

int Stub::decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t** outBuffer, int* outBufferSize)
{
    static int frameNumber = 0;

    assert(outBuffer);
    assert(outBufferSize);

    memset(m_nativeYuvBuffer, 0,
        32 * 32 * ((m_frameWidth + 31) / 32) * ((m_frameHeight + 31) / 32));
    debugDrawCheckerboardYNative(m_nativeYuvBuffer, m_frameWidth, m_frameHeight);

    *outBuffer = m_nativeYuvBuffer;
    *outBufferSize = m_nativeYuvBufferSize;

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++frameNumber;
}

int Stub::decodeToDisplayQueue(
    const CompressedFrame* compressedFrame, int64_t* outPts,
    void **outFrameHandle)
{
    // Do nothing visible.

    assert(outFrameHandle);
    (*outFrameHandle) = nullptr;

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++m_frameNumber;
}

void Stub::displayDecoded(void* handle)
{
    assert(!handle);
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyDecoder* ProxyDecoder::createStub(int frameWidth, int frameHeight)
{
    conf.reload();
    return new Stub(frameWidth, frameHeight);
}
