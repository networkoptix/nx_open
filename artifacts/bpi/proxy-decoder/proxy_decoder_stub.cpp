#include "proxy_decoder.h"

// Alternative impl of ProxyDecoder as a pure software stub which just draws a checkerboard.

#include <cassert>
#include <cstring>

#include "vdpau_session.h"

#define OUTPUT_PREFIX "proxydecoder[STUB]: "
#include "proxy_decoder_utils.h"

namespace {

//-------------------------------------------------------------------------------------------------

/**
 * Used by displayDecoded() to display checkerboard, without hw decoder.
 */
class VdpauHandler
{
public:
    VdpauHandler(int frameWidth, int frameHeight);
    ~VdpauHandler();

    void display(const ProxyDecoder::Rect* rect);

private:
    int m_frameWidth;
    int m_frameHeight;
    int m_videoSurfaceIndex = 0;
    std::vector<VdpVideoSurface> m_vdpVideoSurfaces;
    std::unique_ptr<VdpauSession> m_vdpSession;
};

VdpauHandler::VdpauHandler(int frameWidth, int frameHeight)
:
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight),
    m_vdpSession(new VdpauSession(frameWidth, frameHeight))
{
    for (int i = 0; i < conf.videoSurfaceCount; ++i)
    {
        VdpVideoSurface surface = VDP_INVALID_HANDLE;
        VDP(vdp_video_surface_create(m_vdpSession->vdpDevice(),
            VDP_CHROMA_TYPE_420, frameWidth, frameHeight, &surface));
        vdpCheckHandle(surface, "Video Surface");

        m_vdpVideoSurfaces.push_back(surface);

        static YuvNative yuvNative;
        getVideoSurfaceYuvNative(surface, &yuvNative);
        memset(yuvNative.virt, 0, yuvNative.size);
    }

    PRINT << "VDPAU Initialized OK";
}

VdpauHandler::~VdpauHandler()
{
    OUTPUT << "Deinitializing VDPAU...";

    for (auto& surface: m_vdpVideoSurfaces)
    {
        if (surface != VDP_INVALID_HANDLE)
            VDP(vdp_video_surface_destroy(surface));
        surface = VDP_INVALID_HANDLE;
    }

    m_vdpSession.reset(nullptr); //< To make logs appear in proper place.
}

void VdpauHandler::display(const ProxyDecoder::Rect* rect)
{
    const int videoSurfaceIndex = m_videoSurfaceIndex;
    m_videoSurfaceIndex = (m_videoSurfaceIndex + 1) % conf.videoSurfaceCount;

    VdpVideoSurface videoSurface = m_vdpVideoSurfaces[videoSurfaceIndex];
    assert(videoSurface != VDP_INVALID_HANDLE);

    OUTPUT << "VdpauHandler::display() BEGIN";
    OUTPUT << stringFormat("Using videoSurface %02d of %d {handle #%02d}",
        videoSurfaceIndex, conf.videoSurfaceCount, videoSurface);

    static YuvNative yuvNative;
    getVideoSurfaceYuvNative(videoSurface, &yuvNative);
    memset(yuvNative.virt, 0, yuvNative.luma_size);

    if (conf.enableStubSurfaceNumbers)
    {
        // Print videoSurfaceIndex in its individual position.
        debugPrintNative((uint8_t*) yuvNative.virt, m_frameWidth, m_frameHeight,
            /*x*/ 12 * (videoSurfaceIndex % 4), /*y*/ 6 * (videoSurfaceIndex / 4),
            stringFormat("%02d", videoSurface).c_str());
    }
    else
    {
        debugDrawCheckerboardYNative((uint8_t*) yuvNative.virt, m_frameWidth, m_frameHeight);
    }

    m_vdpSession->displayVideoSurface(videoSurface, rect);

    OUTPUT << "VdpauHandler::display() END";
}

//-------------------------------------------------------------------------------------------------

class Stub
:
    public ProxyDecoder
{
public:
    Stub(int frameWidth, int frameHeight);
    virtual ~Stub() override;

    virtual int decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t* argbBuffer, int argbLineSize) override;

    virtual int decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize) override;

    virtual int decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t** outBuffer, int* outBufferSize) override;

    virtual int decodeToDisplayQueue(
        const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        void **outFrameHandle) override;

    virtual void displayDecoded(void* frameHandle, const Rect* rect) override;

private:
    int m_frameWidth;
    int m_frameHeight;

    uint8_t* m_nativeYuvBuffer = nullptr;
    int m_nativeYuvBufferSize = 0;

    int m_frameNumber;

    static const int kPtsCount = 64;
    int m_ptsIndex = 0;
    int64_t m_ptsQueue[kPtsCount];

    std::unique_ptr<VdpauHandler> m_vdpHandler;

private:
    int obtainResult(const CompressedFrame* compressedFrame, int64_t* outPtsUs);
};

Stub::Stub(int frameWidth, int frameHeight)
:
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight),
    m_frameNumber(0)
{
    assert(frameWidth > 0);
    assert(frameHeight > 0);

    OUTPUT << "Stub(frameWidth: " << frameWidth << ", frameHeight: " << frameHeight << ") "
        << (conf.disable ? "DO NOTHING: conf.disable" : "");

    // Native buffer is NV12 (12 bits per pixel), arranged in 32x32 px macroblocks.
    m_nativeYuvBufferSize =
        ((frameWidth + 15) / 16 * 16) *
        ((frameHeight + 15) / 16 * 16) * 3 / 2;

    m_nativeYuvBuffer = (uint8_t*) malloc(m_nativeYuvBufferSize);
    memset(m_nativeYuvBuffer, 0, m_nativeYuvBufferSize);
}

Stub::~Stub()
{
    OUTPUT << "~Stub() BEGIN";
    free(m_nativeYuvBuffer);
    OUTPUT << "~Stub() END";
}

int Stub::obtainResult(const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPtsUs)
{
    if (compressedFrame)
    {
        if (outPtsUs)
            *outPtsUs = compressedFrame->ptsUs;
        return ++m_frameNumber;
    }
    else
    {
        return 0;
    }
}

int Stub::decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    uint8_t* argbBuffer, int argbLineSize)
{
    OUTPUT << "decodeToRgb(argbLineSize: " << argbLineSize << ")";
    assert(argbBuffer);
    assert(argbLineSize > 0);

    if (!conf.disable)
    {
        memset(argbBuffer, 0, argbLineSize * m_frameHeight);
        debugDrawCheckerboardArgb(argbBuffer, argbLineSize, m_frameWidth, m_frameHeight);
    }

    return obtainResult(compressedFrame, outPtsUs);
}

int Stub::decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    OUTPUT << "decodeToYuvPlanar(yLineSize: " << yLineSize << ", uVLineSize: " << uVLineSize << ")";
    assert(yBuffer);
    assert(uBuffer);
    assert(vBuffer);
    assert(yLineSize > 0);
    assert(uVLineSize > 0);

    if (!conf.disable)
    {
        memset(yBuffer, 0, yLineSize * m_frameHeight);
        debugDrawCheckerboardY(yBuffer, yLineSize, m_frameWidth, m_frameHeight);

        memset(uBuffer, 0, uVLineSize * (m_frameHeight / 2));
        memset(vBuffer, 0, uVLineSize * (m_frameHeight / 2));
    }

    return obtainResult(compressedFrame, outPtsUs);
}

int Stub::decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    uint8_t** outBuffer, int* outBufferSize)
{
    OUTPUT << "decodeToYuvNative()";
    assert(outBuffer);
    assert(outBufferSize);

    if (!conf.disable)
    {
        memset(m_nativeYuvBuffer, 0,
            32 * 32 * ((m_frameWidth + 31) / 32) * ((m_frameHeight + 31) / 32));
        debugDrawCheckerboardYNative(m_nativeYuvBuffer, m_frameWidth, m_frameHeight);
    }
    *outBuffer = m_nativeYuvBuffer;
    *outBufferSize = m_nativeYuvBufferSize;

    return obtainResult(compressedFrame, outPtsUs);
}

int Stub::decodeToDisplayQueue(
    const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    void **outFrameHandle)
{
    OUTPUT << "decodeToDisplayQueue()";
    assert(outFrameHandle);
    *outFrameHandle = nullptr;

    if (compressedFrame)
    {
        m_ptsQueue[m_ptsIndex] = compressedFrame->ptsUs;
        *outFrameHandle = &m_ptsQueue[m_ptsIndex];
        m_ptsIndex = (m_ptsIndex + 1) % kPtsCount;
    }

    return obtainResult(compressedFrame, outPtsUs);
}

void Stub::displayDecoded(void* frameHandle, const Rect* rect)
{
    OUTPUT << "displayDecoded(" << (frameHandle ? "frameHandle" : "nullptr") << ", rect: "
        << (rect
            ? stringFormat(
                "{x: %d, y: %d, width: %d, height: %d}",
                rect->x, rect->y, rect->width, rect->height)
            : "nullptr")
        << ")";

    if (!frameHandle)
    {
        OUTPUT << "displayDecoded() END";
        return;
    }
    int64_t ptsUs = *((int64_t*) frameHandle);

    if (!conf.disable)
    {
        if (!m_vdpHandler)
            m_vdpHandler.reset(new VdpauHandler(m_frameWidth, m_frameHeight));
    }

    if (conf.enableFps)
    {
        static int64_t prevPtsUs = 0;
        std::string ptsStr;
        if (prevPtsUs != 0)
            ptsStr = stringFormat("dPTS %3d;", int((500 + ptsUs - prevPtsUs) / 1000));
        prevPtsUs = ptsUs;

        NX_SHOW_FPS("displayDecoded", ptsStr.c_str());
    }

    if (m_vdpHandler)
        m_vdpHandler->display(rect);

    OUTPUT << "displayDecoded() END";
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyDecoder* ProxyDecoder::createStub(int frameWidth, int frameHeight)
{
    conf.reload();
    return new Stub(frameWidth, frameHeight);
}
