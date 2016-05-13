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

#include "vdpau_helper.h"

#define OUTPUT_PREFIX "ProxyDecoder: "
#include "proxy_decoder_utils.h"

namespace {

static const char* const DEBUG_FRAME_PATH = "/tmp";

class Impl
:
    public ProxyDecoder
{
public:
    Impl(int frameWidth, int frameHeight);
    virtual ~Impl() override;

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
    struct InitOnce
    {
        InitOnce();
    };

    void initializeVdpau();
    void deinitializeVdpau();

    void initializeFfmpeg();
    void deinitializeFfmpeg();

    vdpau_render_state* getRenderState();

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
    int decodeFrame(const CompressedFrame* compressedFrame, int64_t* outPts,
        vdpau_render_state** outRenderState);

    static void fillAvPacket(
        AVPacket* packet, const CompressedFrame* compressedFrame, uint64_t* pLastPts);

    void convertYuvToRgb(uint8_t* argbBuffer, int argbLineSize);

private:
    int m_frameWidth;
    int m_frameHeight;

    int m_renderStateIndex = 0;
    std::vector<vdpau_render_state*> m_renderStates;

    VdpDevice m_vdpDevice = VDP_INVALID_HANDLE;
    VdpDecoder m_vdpDecoder = VDP_INVALID_HANDLE;
    VdpVideoMixer m_vdpMixer = VDP_INVALID_HANDLE;
    VdpPresentationQueueTarget m_vdpTarget = VDP_INVALID_HANDLE;
    VdpPresentationQueue m_vdpQueue = VDP_INVALID_HANDLE;

    AVBufferRef* m_stubAvBuffer = nullptr;
    AVCodecContext* m_codecContext = nullptr;

    uint64_t m_lastPts = AV_NOPTS_VALUE;

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

Impl::Impl(int frameWidth, int frameHeight)
:
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight)
{
    static InitOnce initOnce;
    (void) initOnce;

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
    PRINT << "Initializing VDPAU...";

    m_vdpDevice = createVdpDevice();

    VDP(vdp_decoder_create(m_vdpDevice,
        VDP_DECODER_PROFILE_H264_HIGH,
        m_frameWidth,
        m_frameHeight,
        /*max_references*/ 16,
        &m_vdpDecoder));
    vdpCheckHandle(m_vdpDecoder, "Decoder");

    // The following initializations are needed for decodeToDisplay().

    VDP(vdp_video_mixer_create(m_vdpDevice,
        /*feature_count*/ 0,
        /*features*/ nullptr,
        /*parameter_count*/ 0,
        /*parameters*/ nullptr,
        /*parameter_values*/ nullptr,
        &m_vdpMixer));
    vdpCheckHandle(m_vdpMixer, "Mixer");

    VDP(vdp_presentation_queue_target_create_x11(m_vdpDevice, /*drawable*/ 0, &m_vdpTarget));
    vdpCheckHandle(m_vdpTarget, "Presentation Queue Target");

    VDP(vdp_presentation_queue_create(m_vdpDevice, m_vdpTarget, &m_vdpQueue));
    vdpCheckHandle(m_vdpQueue, "Presentation Queue");

    PRINT << "VDPAU Initialized OK";
}

void Impl::deinitializeVdpau()
{
    OUTPUT << "deinitializeVdpau() BEGIN";

    if (m_vdpQueue != VDP_INVALID_HANDLE)
        VDP(vdp_presentation_queue_destroy(m_vdpQueue));

    if (m_vdpTarget != VDP_INVALID_HANDLE)
        VDP(vdp_presentation_queue_target_destroy(m_vdpTarget));

    if (m_vdpMixer != VDP_INVALID_HANDLE)
        VDP(vdp_video_mixer_destroy(m_vdpMixer));

    if (m_vdpDecoder != VDP_INVALID_HANDLE)
        VDP(vdp_decoder_destroy(m_vdpDecoder));

    for (int i = 0; i < m_renderStates.size(); ++i)
    {
        if (m_renderStates[i]->surface != VDP_INVALID_HANDLE)
            VDP(vdp_video_surface_destroy(m_renderStates[i]->surface));
        delete m_renderStates[i];
    }

    if (m_vdpDevice != VDP_INVALID_HANDLE)
        VDP(vdp_device_destroy(m_vdpDevice));

    OUTPUT << "deinitializeVdpau() END";
}

void Impl::initializeFfmpeg()
{
    AVCodec* codec = avcodec_find_decoder_by_name("h264_vdpau");
    if (!codec)
    {
        PRINT << "avcodec_find_decoder_by_name(\"h264_vdpau\") failed";
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
        PRINT << "avcodec_open2() failed with status " << res;
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

vdpau_render_state* Impl::getRenderState()
{
    vdpau_render_state* renderState = nullptr;

    static const int kRenderStatesCount = 20;

    while (m_renderStates.size() < kRenderStatesCount)
    {
        renderState = new vdpau_render_state();
        m_renderStates.push_back(renderState);
        memset(renderState, 0, sizeof(vdpau_render_state));
        renderState->surface = VDP_INVALID_HANDLE;

        VDP(vdp_video_surface_create(m_vdpDevice,
            VDP_CHROMA_TYPE_420, m_frameWidth, m_frameHeight, &renderState->surface));
        vdpCheckHandle(renderState->surface, "Video Surface");
    }

    renderState = m_renderStates[m_renderStateIndex];
    OUTPUT << "getRenderState(): use vdpau_render_state #" << m_renderStateIndex << " {flags: "
            << debugDumpRenderStateFlags(renderState) <<  "}";
    m_renderStateIndex = (m_renderStateIndex + 1) % kRenderStatesCount;

    assert(renderState);
    return renderState;
}

void Impl::doDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame,
    int offset[4], int y, int type, int height)
{
    (void) pContext;
    (void) offset;
    (void) y;
    (void) type;
    (void) height;

    assert(pFrame);
    vdpau_render_state* renderState = reinterpret_cast<vdpau_render_state*>(pFrame->data[0]);
    if (!renderState)
    {
        OUTPUT << "pFrame->data[0] is null instead of vdpau_render_state*";
        return;
    }
    if (renderState->surface == VDP_INVALID_HANDLE)
    {
        OUTPUT << "((vdpau_render_state*) pFrame->data[0])->surface is VDP_INVALID_HANDLE";
        return;
    }

    TIME_BEGIN(vdp_decoder_render);
    VDP(vdp_decoder_render(
        m_vdpDecoder,
        renderState->surface,
        (VdpPictureInfo const*) &renderState->info,
        renderState->bitstream_buffers_used,
        renderState->bitstream_buffers));
    TIME_END(vdp_decoder_render);
}

int Impl::doGetBuffer2(AVCodecContext* pContext, AVFrame* pFrame, int flags)
{
    (void) pContext;

    if (flags & AV_GET_BUFFER_FLAG_REF)
    OUTPUT << "AV_GET_BUFFER_FLAG_REF is set";

    vdpau_render_state* renderState = getRenderState();
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
    OUTPUT << "callbackGetBuffer2() BEGIN";
    Impl* d = (Impl*) pContext->opaque;
    assert(d);
    int result = d->doGetBuffer2(pContext, pFrame, flags);
    OUTPUT << "callbackGetBuffer2() END";
    return result;
}

void Impl::callbackDrawHorizBand(struct AVCodecContext* pContext, const AVFrame* src,
    int offset[4], int y, int type, int height)
{
    OUTPUT << "callbackDrawHorizBand() BEGIN";
    Impl* d = (Impl*) pContext->opaque;
    assert(d);
    d->doDrawHorizBand(pContext, src, offset, y, type, height);
    OUTPUT << "callbackDrawHorizBand() END";
}

AVPixelFormat Impl::callbackGetFormat(AVCodecContext* pContext, const AVPixelFormat* pFmt)
{
    OUTPUT << "callbackGetFormat()";

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
    AVPacket* packet, const CompressedFrame* compressedFrame, uint64_t* pLastPts)
{
    assert(packet);
    assert(pLastPts);

    av_init_packet(packet);
    if (compressedFrame)
    {
        assert(compressedFrame->data);
        assert(compressedFrame->dataSize > 0);
        packet->data = (unsigned char*) compressedFrame->data;
        packet->size = compressedFrame->dataSize;
        packet->dts = packet->pts = compressedFrame->pts;
        if (compressedFrame->isKeyFrame)
            packet->flags = AV_PKT_FLAG_KEY;

        // Zero padding required by ffmpeg.
        static_assert(CompressedFrame::kPaddingSize >= AV_INPUT_BUFFER_PADDING_SIZE,
            "ProxyDecoder: Insufficient padding size");
        memset(packet->data + packet->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

        *pLastPts = compressedFrame->pts;
    }
    else
    {
        // There is a known ffmpeg bug. It returns the below time for the very last packet while
        // flushing internal buffer. So, repeat this time for the empty packet in order to avoid
        // the bug.
        packet->pts = packet->dts = *pLastPts;
        packet->data = nullptr;
        packet->size = 0;
    }
}

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

void Impl::convertYuvToRgb(uint8_t* argbBuffer, int argbLineSize)
{
    // Software conversion of YUV to RGB.
    // Measured time for Full-HD frame:
    // 440 ms YUV FULL
    //  60 ms YUV PART
    //  86 ms Y_ONLY FULL
    //  12 ms Y_ONLY PART

    const int y0 = conf.enableRgbPartOnly ? (m_frameHeight / 3) : 0;
    const int yEnd = conf.enableRgbPartOnly ? (m_frameHeight * 2 / 3) : m_frameHeight;
    const int x0 = conf.enableRgbPartOnly ? (m_frameWidth / 3) : 0;
    const int xEnd = conf.enableRgbPartOnly ? (m_frameWidth * 2 / 3) : m_frameWidth;

    uint8_t* pYSrc = m_yBufferStart + m_yLineSize * y0;
    uint8_t* pDestLine = argbBuffer + argbLineSize * y0;

    uint8_t* pVSrc = m_vBufferStart + m_uVLineSize * y0;
    uint8_t* pUSrc = m_uBufferStart + m_uVLineSize * y0;

    for (int y = y0; y < yEnd; ++y)
    {
        if (conf.enableRgbYOnly)
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

int Impl::decodeFrame(const CompressedFrame* compressedFrame, int64_t* outPts,
    vdpau_render_state** outRenderState)
{
    assert(outRenderState);
    *outRenderState = nullptr;

    AVPacket avpkt;
    fillAvPacket(&avpkt, compressedFrame, &m_lastPts);

    AVFrame* avFrame = av_frame_alloc();

    OUTPUT << "avcodec_decode_video2() BEGIN";
    TIME_BEGIN(avcodec_decode_video2);
    int gotPicture = 0;
    int res = avcodec_decode_video2(m_codecContext, avFrame, &gotPicture, &avpkt);
    TIME_END(avcodec_decode_video2);
    OUTPUT << "avcodec_decode_video2() END -> " << res << "; gotPicture: " << gotPicture << "";

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
                // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert us to ms.
                if (outPts)
                    *outPts = avFrame->pkt_dts / 1000;

                result = avFrame->coded_picture_number;
            }
        }
    }

    av_frame_free(&avFrame);
    return result;
}

int Impl::decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    OUTPUT << "decodeToRgb(argbLineSize: " << argbLineSize << ") BEGIN";

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

    int result = decodeToYuvPlanar(compressedFrame, outPts,
        m_yBufferStart, m_yLineSize, m_uBufferStart, m_vBufferStart,
        m_uVLineSize);
    if (result > 0)
    {
        TIME_BEGIN(convertYuvToRgb);
        convertYuvToRgb(argbBuffer, argbLineSize);
        TIME_END(convertYuvToRgb);
    }
    OUTPUT << "decodeToRgb() END -> " << result;
    return result;
}

int Impl::decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    OUTPUT << "decodeToYuvPlanar(yLineSize: " << yLineSize
        << ", uVLineSize: " << uVLineSize << ") BEGIN";

    assert(yBuffer);
    assert(vBuffer);
    assert(uBuffer);
    assert(yLineSize > 0);
    assert(uVLineSize > 0);

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPts, &renderState);
    if (result > 0)
    {
        if (!conf.disableGetBits)
        {
            void* const dest[3] = {yBuffer, vBuffer, uBuffer};
            const uint32_t pitches[3] = {
                (uint32_t) yLineSize, (uint32_t) uVLineSize, (uint32_t) uVLineSize};
            assert(renderState->surface != VDP_INVALID_HANDLE);

            TIME_BEGIN(vdp_video_surface_get_bits_y_cb_cr);
            VDP(vdp_video_surface_get_bits_y_cb_cr(renderState->surface,
                VDP_YCBCR_FORMAT_YV12, dest, pitches));
            TIME_END(vdp_video_surface_get_bits_y_cb_cr);

            if (conf.enableYuvDump)
            {
                debugDumpYuvSurfaceToFiles(DEBUG_FRAME_PATH, renderState->surface,
                    yBuffer, yLineSize, uBuffer, vBuffer, uVLineSize);
            }
        }

        //renderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE; //< Old approach.
    }
    OUTPUT << "decodeToYuvPlanar() END -> " << result;
    return result;
}

int Impl::decodeToYuvNative(
    const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t** outBuffer, int* outBufferSize)
{
    OUTPUT << "decodeToYuvNative() BEGIN";

    assert(outBuffer);
    assert(outBufferSize);
    *outBuffer = nullptr;
    *outBufferSize = 0;

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPts, &renderState);
    if (result > 0)
    {
        YuvNative yuvNative;
        assert(renderState->surface != VDP_INVALID_HANDLE);
        getVideoSurfaceYuvNative(renderState->surface, &yuvNative);
        if (conf.enableLogYuvNative)
            logYuvNative(&yuvNative);

        *outBuffer = (uint8_t*) yuvNative.virt;
        *outBufferSize = (int) yuvNative.size;

        //renderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE; //< Old approach.
    }
    OUTPUT << "decodeToYuvNative() END -> " << result;
    return result;
}

int Impl::decodeToDisplayQueue(const CompressedFrame* compressedFrame, int64_t* outPts,
    void** outFrameHandle)
{
    OUTPUT << "decodeForDisplaying() BEGIN";
    assert(outFrameHandle);
    *outFrameHandle = nullptr;

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPts, &renderState);
    if (result > 0)
        *outFrameHandle = reinterpret_cast<void*>(renderState);

    OUTPUT << "decodeForDisplaying() END -> " << result;
    return result;
}

void Impl::displayDecoded(void* frameHandle)
{
    if (!frameHandle)
        return;
    vdpau_render_state* renderState = reinterpret_cast<vdpau_render_state*>(frameHandle);

    OUTPUT << "displayDecoded(" << debugDumpRenderStateRef(renderState, m_renderStates) << ") BEGIN";
    assert(renderState->surface != VDP_INVALID_HANDLE);

// TODO mike: REMOVE
#if 0
static YuvNative yuvNative;
getVideoSurfaceYuvNative(renderState->surface, &yuvNative);
////memset(yuvNative.virt, 0, yuvNative.luma_size);
debugDrawCheckerboardYNative((uint8_t*) yuvNative.virt, m_frameWidth, m_frameHeight);
#endif // 0

    if (conf.enableFps)
    {
        static uint32_t prevHash = 0;
        static YuvNative yuvNative;
        getVideoSurfaceYuvNative(renderState->surface, &yuvNative);
        uint32_t curHash = calcYuvNativeQuickHash(&yuvNative);
        debugShowFps((
            stringFormat("{%2d} ", renderState->surface)
            + ((curHash == prevHash) ? "========" : stringFormat("%08X", curHash))
            + " ").c_str());
        prevHash = curHash;
    }

    VdpOutputSurface outputSurface;
    VDP(vdp_output_surface_create(m_vdpDevice,
        VDP_RGBA_FORMAT_B8G8R8A8, m_frameWidth, m_frameHeight, &outputSurface));
    vdpCheckHandle(outputSurface, "Output Surface");

    // In vdpau_sunxi, this links Video Surface to Output Surface; does not process pixel data.
    VDP(vdp_video_mixer_render(m_vdpMixer,
        /*background_surface*/ VDP_INVALID_HANDLE, //< Not implemented.
        /*background_source_rect*/ nullptr, //< Not implemented.
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME, //< Other values not implemented.
        /*video_surfaces_past_count*/ 0, //< Not implemented.
        /*video_surfaces_past*/ nullptr, //< Not implemented.
        /*video_surface_current*/ renderState->surface,
        /*video_surfaces_future_count*/ 0, //< Not implemented.
        /*video_surfaces_future*/ nullptr, //< Not implemented.
        /*video_source_rect*/ nullptr, //< Can be specified.
        outputSurface,
        /*destination_rect*/ nullptr, //< Not implemented.
        /*destination_video_rect*/ nullptr, //< Can be specified.
        /*layer_count*/ 0, //< Other values not implemented.
        /*layers*/ nullptr //< Not implemented.
    ));

    VDP(vdp_presentation_queue_display(m_vdpQueue, outputSurface,
        /*clip_width*/ m_frameWidth, /*clip_height*/ m_frameHeight,
        /*earliest_presentation_time, not implemented*/ 0));

    VDP(vdp_output_surface_destroy(outputSurface));

    //renderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE; //< Old approach.

    OUTPUT << "displayDecoded(" << debugDumpRenderStateRef(renderState, m_renderStates) << ") END";
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyDecoder* ProxyDecoder::createImpl(int frameWidth, int frameHeight)
{
    conf.reload();
    return new Impl(frameWidth, frameHeight);
}
