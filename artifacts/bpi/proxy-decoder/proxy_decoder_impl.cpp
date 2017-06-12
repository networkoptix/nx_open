#include "proxy_decoder.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>

extern "C" {
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/vdpau.h>
} // extern "C"

#include "vdpau_session.h"

#include "proxy_decoder_utils.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "proxydecoder: "

namespace {

class Impl: public ProxyDecoder
{
public:
    Impl(int frameWidth, int frameHeight);
    virtual ~Impl() override;

    virtual int decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t* argbBuffer, int argbLineSize) override;

    virtual int decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize) override;

    virtual int decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        uint8_t** outBuffer, int* outBufferSize) override;

    virtual int decodeToDisplayQueue(
        const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        void **outFrameHandle) override;

    virtual void displayDecoded(void* frameHandle, const ProxyDecoder::Rect* rect) override;

private:
    struct InitOnce
    {
        InitOnce();
    };

    void initializeVdpau();
    void deinitializeVdpau();

    void initializeFfmpeg();
    void deinitializeFfmpeg();

    int doGetBuffer2(AVCodecContext* pContext, AVFrame* pFrame, int flags);
    void doDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame,
        int offset[4], int y, int type, int height);

    // Ffmpeg callbacks:
    static int callbackGetBuffer2(AVCodecContext* pContext, AVFrame* pFrame, int flags);
    static void callbackDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame,
        int offset[4], int y, int type, int height);
    static AVPixelFormat callbackGetFormat(AVCodecContext* pContext, const AVPixelFormat* pFmt);

    /**
     * @return Value with the same semantics as ProxyDecoder::decodeTo...().
     */
    int decodeFrame(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
        vdpau_render_state** outRenderState);

    static void fillAvPacket(
        AVPacket* packet, const CompressedFrame* compressedFrame, int64_t* pLastPtsUs);

    void convertYuvToRgb(uint8_t* argbBuffer, int argbLineSize);

    std::string getFrameEqualsPrevStr(const vdpau_render_state* renderState) const;

private:
    int m_frameWidth;
    int m_frameHeight;

    int m_renderStateIndex = 0;
    std::vector<vdpau_render_state*> m_renderStates;

    VdpDecoder m_vdpDecoder = VDP_INVALID_HANDLE;

    std::unique_ptr<VdpauSession> m_vdpSession;

    AVBufferRef* m_stubAvBuffer = nullptr;
    AVCodecContext* m_codecContext = nullptr;

    int64_t m_lastPtsUs = AV_NOPTS_VALUE;

    // Buffers for decodeToRgb().
    bool m_yuvBuffersAllocated = false;
    uint8_t* m_yBuffer = nullptr;
    uint8_t* m_uBuffer = nullptr;
    uint8_t* m_vBuffer = nullptr;
    uint8_t* m_yBufferStart = nullptr;
    uint8_t* m_uBufferStart = nullptr;
    uint8_t* m_vBufferStart = nullptr;
    int m_yLineSize = 0;
    int m_uVLineSize = 0;
};

Impl::InitOnce::InitOnce()
{
    av_register_all();
}

Impl::Impl(int frameWidth, int frameHeight):
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight)
{
    static InitOnce initOnce;
    /*unused*/ (void) initOnce;

    assert(m_frameWidth > 0);
    assert(m_frameHeight > 0);

    initializeVdpau();
    initializeFfmpeg();
}

Impl::~Impl()
{
    deinitializeFfmpeg();
    deinitializeVdpau();

    free(m_yBuffer);
    free(m_uBuffer);
    free(m_vBuffer);
}

void Impl::initializeVdpau()
{
    m_vdpSession.reset(new VdpauSession(m_frameWidth, m_frameHeight));

    VDP(vdp_decoder_create(m_vdpSession->vdpDevice(),
        VDP_DECODER_PROFILE_H264_HIGH,
        m_frameWidth,
        m_frameHeight,
        /*max_references*/ ini().videoSurfaceCount, //< Used only for checking to be <= 16.
        &m_vdpDecoder));
    vdpCheckHandle(m_vdpDecoder, "Decoder");

    for (int i = 0; i < ini().videoSurfaceCount; ++i)
    {
        vdpau_render_state* renderState = new vdpau_render_state();
        m_renderStates.push_back(renderState);
        memset(renderState, 0, sizeof(vdpau_render_state));
        renderState->surface = VDP_INVALID_HANDLE;

        VDP(vdp_video_surface_create(m_vdpSession->vdpDevice(),
            VDP_CHROMA_TYPE_420, m_frameWidth, m_frameHeight, &renderState->surface));
        vdpCheckHandle(renderState->surface, "Video Surface");
    }

    NX_PRINT << "VDPAU Initialized OK";
}

void Impl::deinitializeVdpau()
{
    NX_OUTPUT << "Deinitializing VDPAU...";

    for (auto& renderState: m_renderStates)
    {
        if (renderState->surface != VDP_INVALID_HANDLE)
            VDP(vdp_video_surface_destroy(renderState->surface));
        delete renderState;
        renderState = nullptr;
    }

    if (m_vdpDecoder != VDP_INVALID_HANDLE)
        VDP(vdp_decoder_destroy(m_vdpDecoder));

    m_vdpSession.reset(nullptr); //< To make logs appear in proper place.
}

void Impl::initializeFfmpeg()
{
    AVCodec* codec = avcodec_find_decoder_by_name("h264_vdpau");
    if (!codec)
    {
        NX_PRINT << "ERROR: avcodec_find_decoder_by_name(\"h264_vdpau\") failed";
        assert(false);
        return;
    }

    m_codecContext = avcodec_alloc_context3(codec);

    m_codecContext->opaque = this;

    m_codecContext->get_buffer2 = Impl::callbackGetBuffer2;
    m_codecContext->draw_horiz_band = Impl::callbackDrawHorizBand;
    m_codecContext->get_format = Impl::callbackGetFormat;
    m_codecContext->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;

    int res = avcodec_open2(m_codecContext, codec, nullptr);
    if (res < 0)
    {
        NX_PRINT << "ERROR: avcodec_open2() failed with status " << res;
        assert(false);
        return;
    }

    m_stubAvBuffer = av_buffer_alloc(/*size*/ 1);
}

void Impl::deinitializeFfmpeg()
{
    if (m_stubAvBuffer)
        av_buffer_unref(&m_stubAvBuffer);

    if (m_codecContext)
    {
        avcodec_close(m_codecContext);
        av_freep(&m_codecContext->rc_override);
        av_freep(&m_codecContext->intra_matrix);
        av_freep(&m_codecContext->inter_matrix);
        av_freep(&m_codecContext->extradata);
        //av_freep(&m_codecContext->rc_eq); //< Deprecated.
        av_freep(&m_codecContext);
        m_codecContext = nullptr;
    }
}

void Impl::doDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame,
    int offset[4], int y, int type, int height)
{
    /*unused*/ (void) pContext;
    /*unused*/ (void) offset;
    /*unused*/ (void) y;
    /*unused*/ (void) type;
    /*unused*/ (void) height;

    assert(pFrame);
    vdpau_render_state* renderState = reinterpret_cast<vdpau_render_state*>(pFrame->data[0]);
    if (!renderState)
    {
        NX_OUTPUT << "pFrame->data[0] is null instead of vdpau_render_state*";
        return;
    }
    if (renderState->surface == VDP_INVALID_HANDLE)
    {
        NX_OUTPUT << "((vdpau_render_state*) pFrame->data[0])->surface is VDP_INVALID_HANDLE";
        return;
    }

    NX_TIME_BEGIN(vdp_decoder_render);
    VDP(vdp_decoder_render(
        m_vdpDecoder,
        renderState->surface,
        (VdpPictureInfo const*) &renderState->info,
        renderState->bitstream_buffers_used,
        renderState->bitstream_buffers));
    NX_TIME_END(vdp_decoder_render);
}

int Impl::doGetBuffer2(AVCodecContext* pContext, AVFrame* pFrame, int flags)
{
    (void) pContext;

    if (flags & AV_GET_BUFFER_FLAG_REF)
    {
        NX_OUTPUT << "AV_GET_BUFFER_FLAG_REF is set";
    }

    vdpau_render_state* renderState = m_renderStates[m_renderStateIndex];
    NX_OUTPUT << "getRenderState(): use vdpau_render_state #" << m_renderStateIndex
        << " {flags: " << debugDumpRenderStateFlagsToStr(renderState) <<  "}";
    m_renderStateIndex = (m_renderStateIndex + 1) % ini().videoSurfaceCount;
    assert(renderState);

    pFrame->buf[0] = av_buffer_ref(m_stubAvBuffer);
    pFrame->data[0] = reinterpret_cast<uint8_t*>(renderState);
    pFrame->extended_data = pFrame->data;
    pFrame->linesize[0] = 0;
    pFrame->extended_buf = nullptr;
    pFrame->nb_extended_buf = 0;

    //renderState->state |= FF_VDPAU_STATE_USED_FOR_REFERENCE; //< Old approach.
    return 0;
}

int Impl::callbackGetBuffer2(AVCodecContext* pContext, AVFrame* pFrame, int flags)
{
    NX_OUTPUT << "callbackGetBuffer2() BEGIN";
    Impl* d = (Impl*) pContext->opaque;
    assert(d);
    int result = d->doGetBuffer2(pContext, pFrame, flags);
    NX_OUTPUT << "callbackGetBuffer2() END";
    return result;
}

void Impl::callbackDrawHorizBand(struct AVCodecContext* pContext, const AVFrame* src,
    int offset[4], int y, int type, int height)
{
    NX_OUTPUT << "callbackDrawHorizBand() BEGIN";
    Impl* d = (Impl*) pContext->opaque;
    assert(d);
    d->doDrawHorizBand(pContext, src, offset, y, type, height);
    NX_OUTPUT << "callbackDrawHorizBand() END";
}

AVPixelFormat Impl::callbackGetFormat(AVCodecContext* pContext, const AVPixelFormat* pFmt)
{
    NX_OUTPUT << "callbackGetFormat()";

    switch (pContext->codec_id)
    {
        case AV_CODEC_ID_H264:
            return AV_PIX_FMT_VDPAU_H264;
        case AV_CODEC_ID_MPEG1VIDEO:
            return AV_PIX_FMT_VDPAU_MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO:
            return AV_PIX_FMT_VDPAU_MPEG2;
        case AV_CODEC_ID_WMV3:
            return AV_PIX_FMT_VDPAU_WMV3;
        case AV_CODEC_ID_VC1:
            return AV_PIX_FMT_VDPAU_VC1;
        default:
            return pFmt[0];
    }
}

void Impl::fillAvPacket(
    AVPacket* packet, const CompressedFrame* compressedFrame, int64_t* pLastPtsUs)
{
    assert(packet);
    assert(pLastPtsUs);

    av_init_packet(packet);
    if (compressedFrame)
    {
        assert(compressedFrame->data);
        assert(compressedFrame->dataSize > 0);
        packet->data = (unsigned char*) compressedFrame->data;
        packet->size = compressedFrame->dataSize;
        packet->dts = packet->pts = compressedFrame->ptsUs;
        if (compressedFrame->isKeyFrame)
            packet->flags = AV_PKT_FLAG_KEY;

        // Zero padding required by ffmpeg.
        static_assert(CompressedFrame::kPaddingSize >= AV_INPUT_BUFFER_PADDING_SIZE,
            "ProxyDecoder: Insufficient padding size");
        memset(packet->data + packet->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

        *pLastPtsUs = compressedFrame->ptsUs;
    }
    else
    {
        // There is a known ffmpeg bug. It returns the below time for the very last packet while
        // flushing internal buffer. So, repeat this time for the empty packet in order to avoid
        // the bug.
        packet->pts = packet->dts = *pLastPtsUs;
        packet->data = nullptr;
        packet->size = 0;
    }
}

namespace {

static void inline convertPixelYuvToRgb(uint8_t* p, int y, int u, int v)
{
    // u = Cb, v = Cr
    int u1 = u - 128;
    int v1 = v - 128;
    int tempy = 298 * (y - 16);
    int b = (tempy + 516 * u1) >> 8;
    int g = (tempy - 100 * u1 - 208 * v1) >> 8;
    int r = (tempy + 409 * v1) >> 8;

    if (b < 0) b = 0;
    if (b > 255) b = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (r < 0) r = 0;
    if (r > 255) r = 255;

    p[0] = (uint8_t) b;
    p[1] = (uint8_t) g;
    p[2] = (uint8_t) r;
    p[3] = 0;
}

} // namespace

void Impl::convertYuvToRgb(uint8_t* argbBuffer, int argbLineSize)
{
    // Software conversion of YUV to RGB.
    // Measured time for Full-HD frame:
    // 440 ms YUV FULL
    //  60 ms YUV PART
    //  86 ms Y_ONLY FULL
    //  12 ms Y_ONLY PART

    const int y0 = ini().enableRgbPartOnly ? (m_frameHeight / 3) : 0;
    const int yEnd = ini().enableRgbPartOnly ? (m_frameHeight * 2 / 3) : m_frameHeight;
    const int x0 = ini().enableRgbPartOnly ? (m_frameWidth / 3) : 0;
    const int xEnd = ini().enableRgbPartOnly ? (m_frameWidth * 2 / 3) : m_frameWidth;

    uint8_t* pYSrc = m_yBufferStart + m_yLineSize * y0;
    uint8_t* pDestLine = argbBuffer + argbLineSize * y0;

    uint8_t* pVSrc = m_vBufferStart + m_uVLineSize * y0;
    uint8_t* pUSrc = m_uBufferStart + m_uVLineSize * y0;

    for (int y = y0; y < yEnd; ++y)
    {
        if (ini().enableRgbYOnly)
        {
            for (int x = x0; x < xEnd; ++x)
                ((uint32_t*) pDestLine)[x] = pYSrc[x]; //< Convert byte to 32-bit.
        }
        else
        {
            for (int x = x0; x < xEnd; ++x)
                convertPixelYuvToRgb(pDestLine + x * 4, pYSrc[x], pUSrc[x / 2], pVSrc[x / 2]);
            if (y % 2 == 1)
            {
                pUSrc += m_uVLineSize;
                pVSrc += m_uVLineSize;
            }
        }

        pDestLine += argbLineSize;
        pYSrc += m_yLineSize;
    }
}

int Impl::decodeFrame(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    vdpau_render_state** outRenderState)
{
    assert(outRenderState);
    *outRenderState = nullptr;

    AVPacket avpkt;
    fillAvPacket(&avpkt, compressedFrame, &m_lastPtsUs);

    AVFrame* avFrame = av_frame_alloc();

    NX_OUTPUT << "avcodec_decode_video2() BEGIN";
    NX_TIME_BEGIN(avcodec_decode_video2);
    int gotPicture = 0;
    int res = avcodec_decode_video2(m_codecContext, avFrame, &gotPicture, &avpkt);
    NX_TIME_END(avcodec_decode_video2);
    NX_OUTPUT << "avcodec_decode_video2() END -> " << res << "; gotPicture: " << gotPicture << "";

    // Ffmpeg doc guarantees that avcodec_decode_video2() returns 0 if gotPicture is false.
    int result = 0;
    if (res <= 0)
    {
        result = res; //< Negative value means error, zero means buffering.
    }
    else
    {
        if (gotPicture)
        {
            *outRenderState = reinterpret_cast<vdpau_render_state*>(avFrame->data[0]);
            if (*outRenderState) //< Null may happen if ffmpeg did not call callbackGetBuffer2().
            {
                // Ffmpeg pts/dts are mixed up here, so it's pkt_dts.
                if (outPtsUs)
                    *outPtsUs = avFrame->pkt_dts;

                result = avFrame->coded_picture_number;
            }
        }
    }

    av_frame_free(&avFrame);
    return result;
}

int Impl::decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    uint8_t* argbBuffer, int argbLineSize)
{
    NX_OUTPUT << "decodeToRgb(argbLineSize: " << argbLineSize << ") BEGIN";

    if (!m_yuvBuffersAllocated)
    {
        m_yuvBuffersAllocated = true;

        // TODO: Decide on alignment.
        m_yBuffer = (uint8_t*) malloc(m_frameWidth * m_frameHeight);
        m_yBufferStart = m_yBuffer;
        m_yLineSize = m_frameWidth;
        m_uBuffer = (uint8_t*) malloc((m_frameWidth / 2) * (m_frameHeight / 2));
        m_vBuffer = (uint8_t*) malloc((m_frameWidth / 2) * (m_frameHeight / 2));
        m_uBufferStart = m_uBuffer;
        m_vBufferStart = m_vBuffer;
        m_uVLineSize = m_frameWidth / 2;
    }

    int result = decodeToYuvPlanar(compressedFrame, outPtsUs,
        m_yBufferStart, m_yLineSize, m_uBufferStart, m_vBufferStart,
        m_uVLineSize);
    if (result > 0)
    {
        NX_TIME_BEGIN(convertYuvToRgb);
        convertYuvToRgb(argbBuffer, argbLineSize);
        NX_TIME_END(convertYuvToRgb);
    }
    NX_OUTPUT << "decodeToRgb() END -> " << result;
    return result;
}

int Impl::decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    NX_OUTPUT << "decodeToYuvPlanar(yLineSize: " << yLineSize
        << ", uVLineSize: " << uVLineSize << ") BEGIN";

    assert(yBuffer);
    assert(vBuffer);
    assert(uBuffer);
    assert(yLineSize > 0);
    assert(uVLineSize > 0);

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPtsUs, &renderState);
    if (result > 0)
    {
        if (!ini().disableGetBits)
        {
            void* const dest[3] = {yBuffer, vBuffer, uBuffer};
            const uint32_t pitches[3] = {
                (uint32_t) yLineSize, (uint32_t) uVLineSize, (uint32_t) uVLineSize};
            assert(renderState->surface != VDP_INVALID_HANDLE);

            NX_TIME_BEGIN(vdp_video_surface_get_bits_y_cb_cr);
            VDP(vdp_video_surface_get_bits_y_cb_cr(renderState->surface,
                VDP_YCBCR_FORMAT_YV12, dest, pitches));
            NX_TIME_END(vdp_video_surface_get_bits_y_cb_cr);

            if (ini().enableYuvDump)
            {
                debugDumpYuvSurfaceToFiles(ini().iniFileDir(), renderState->surface,
                    yBuffer, yLineSize, uBuffer, vBuffer, uVLineSize);
            }
        }

        //renderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE; //< Old approach.
    }
    NX_OUTPUT << "decodeToYuvPlanar() END -> " << result;
    return result;
}

int Impl::decodeToYuvNative(
    const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    uint8_t** outBuffer, int* outBufferSize)
{
    NX_OUTPUT << "decodeToYuvNative() BEGIN";

    assert(outBuffer);
    assert(outBufferSize);
    *outBuffer = nullptr;
    *outBufferSize = 0;

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPtsUs, &renderState);
    if (result > 0)
    {
        YuvNative yuvNative;
        assert(renderState->surface != VDP_INVALID_HANDLE);
        getVideoSurfaceYuvNative(renderState->surface, &yuvNative);
        if (ini().enableLogYuvNative)
            logYuvNative(&yuvNative);

        *outBuffer = (uint8_t*) yuvNative.virt;
        *outBufferSize = (int) yuvNative.size;

        //renderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE; //< Old approach.
    }
    NX_OUTPUT << "decodeToYuvNative() END -> " << result;
    return result;
}

int Impl::decodeToDisplayQueue(const CompressedFrame* compressedFrame, int64_t* outPtsUs,
    void** outFrameHandle)
{
    NX_OUTPUT << "decodeForDisplaying() BEGIN";
    assert(outFrameHandle);
    *outFrameHandle = nullptr;

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPtsUs, &renderState);
    if (result > 0)
        *outFrameHandle = reinterpret_cast<void*>(renderState);

    NX_OUTPUT << "decodeForDisplaying() END -> " << result;
    return result;
}

void Impl::displayDecoded(void* frameHandle, const ProxyDecoder::Rect* rect)
{
    NX_OUTPUT << "displayDecoded(" << (frameHandle ? "frameHandle" : "nullptr") << ", rect: "
        << (rect
            ? format(
                "{x: %d, y: %d, width: %d, height: %d}",
                rect->x, rect->y, rect->width, rect->height)
            : "nullptr")
        << ") BEGIN";

    if (!frameHandle)
    {
        NX_OUTPUT << "displayDecoded() END";
        return;
    }
    vdpau_render_state* renderState = reinterpret_cast<vdpau_render_state*>(frameHandle);
    assert(renderState->surface != VDP_INVALID_HANDLE);

    if (ini().enableFpsDisplayDecoded)
    {
        NX_FPS(DisplayDecoded,
            (format("{%2d} ", renderState->surface)
            + getFrameEqualsPrevStr(renderState)).c_str());
    }

    NX_OUTPUT << "Using " << debugDumpRenderStateRefToStr(renderState, m_renderStates);

    m_vdpSession->displayVideoSurface(renderState->surface, rect);

    NX_OUTPUT << "displayDecoded() END: " << debugDumpRenderStateRefToStr(renderState, m_renderStates);
}

std::string Impl::getFrameEqualsPrevStr(const vdpau_render_state* renderState) const
{
    std::string result;
    if (ini().enableFrameHash)
    {
        static uint32_t prevHash = 0;
        static YuvNative yuvNative;
        getVideoSurfaceYuvNative(renderState->surface, &yuvNative);
        const uint32_t curHash = calcYuvNativeQuickHash(&yuvNative);
        if (curHash == prevHash)
            result = "======== ";
        else
            result = format("%08X ", curHash);
        prevHash = curHash;
    }
    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyDecoder* ProxyDecoder::createImpl(int frameWidth, int frameHeight)
{
    ini().reload();
    return new Impl(frameWidth, frameHeight);
}
