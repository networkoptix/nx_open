#include "proxy_decoder.h"

// Configuration
#define xENABLE_LOG
#define xENABLE_LOG_VDPAU
#define xENABLE_STUB //< Draw a checkerboard with pure C++ code.
#define ENABLE_GET_BITS //< If disabled, ...get_bits... will NOT be called (thus no picture).
#define xENABLE_TIME
#define xENABLE_RGB_VDPAU //< Use VDPAU Mixer for YUV->RGB instead of pure C++ code.
#define ENABLE_RGB_Y_ONLY //< Convert only Y to Blue, setting Red and Green to 0.
#define ENABLE_RGB_PART_ONLY //< Convert to RGB only a part of the frame.

#ifdef ENABLE_STUB
#include "proxy_decoder_stub.cxx"
#else // ENABLE_STUB

#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>

extern "C" {
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
} // extern "C"

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>
#include <libavcodec/vdpau.h>

#define PRINT(...) do { std::cerr << __VA_ARGS__ << "\n"; } while(0)

#define LOG_PREFIX "ProxyDecoder"
#include "proxy_decoder_debug.cxx"

#include "vdpau_helper.cxx"

//-------------------------------------------------------------------------------------------------
// ProxyDecoderPrivate

class ProxyDecoderPrivate
{
    ProxyDecoder* q;

public:
    ProxyDecoderPrivate(ProxyDecoder* q, int frameWidth, int frameHeight);

    ~ProxyDecoderPrivate();

    int decodeToYuvPlanar(const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize);

    int decodeToRgb(const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* argbBuffer, int argbLineSize);

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
    void doDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame);

    // Ffmpeg callbacks:
    static int callbackGetBuffer2(AVCodecContext* pContext, AVFrame* pFrame, int flags);
    static void callbackDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame,
        int offset[4], int y, int type, int height);
    static AVPixelFormat callbackGetFormat(AVCodecContext* pContext, const AVPixelFormat* pFmt);

    /**
     * @return Value with the same semantics as ProxyDecoder::decodeTo...().
     */
    int decodeFrame(
        const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
        vdpau_render_state** outRenderState);

    static void fillAvPacket(AVPacket* packet,
        const ProxyDecoder::CompressedFrame* compressedFrame, uint64_t* pLastPts);

    void convertYuvToRgb(uint8_t* argbBuffer, int argbLineSize);

    int decodeToRgbSoftware(const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* argbBuffer, int argbLineSize);

    int decodeToRgbVdpau(const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
        uint8_t* argbBuffer, int argbLineSize);

private:
    int m_frameWidth;
    int m_frameHeight;

    std::vector<vdpau_render_state*> m_renderStates;

    VdpDecoder m_vdpDecoder = VDP_INVALID_HANDLE;
    VdpVideoMixer m_vdpMixer = VDP_INVALID_HANDLE;
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

ProxyDecoderPrivate::InitOnce::InitOnce()
{
    av_register_all();
}

ProxyDecoderPrivate::ProxyDecoderPrivate(ProxyDecoder* q, int frameWidth, int frameHeight)
    :
    q(q),
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight)
{
    static InitOnce initOnce;

    initializeVdpau();
    initializeFfmpeg();
}

ProxyDecoderPrivate::~ProxyDecoderPrivate()
{
    deinitializeFfmpeg();
    deinitializeVdpau();

    free(m_yBuffer);
    free(m_uBuffer);
    free(m_vBuffer);
}

void ProxyDecoderPrivate::initializeVdpau()
{
    PRINT("Initializing VDPAU...");

    const VdpDecoderProfile profile = VDP_DECODER_PROFILE_H264_HIGH;

    if (getVdpDevice() == 0)
    {
        PRINT("ERROR: Unable to get VDPAU device");
        assert(false);
    }

    VDP(vdp_decoder_create(getVdpDevice(),
        profile, m_frameWidth, m_frameHeight, 16, &m_vdpDecoder));

    #ifdef ENABLE_RGB_VDPAU
        const VdpVideoMixerFeature features[] =
        {
            VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
            VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
        };
        const VdpVideoMixerParameter params[] =
        {
            VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
            VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
            VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
            VDP_VIDEO_MIXER_PARAMETER_LAYERS
        };
        VdpChromaType chroma = VDP_CHROMA_TYPE_420;
        int numLayers = 0;
        const void* const paramValues[] = {&m_frameWidth, &m_frameHeight, &chroma, &numLayers};

        VDP(vdp_video_mixer_create(getVdpDevice(),
            2, features, 4, params, paramValues, &m_vdpMixer));
    #endif // ENABLE_RGB_VDPAU

    PRINT("VDPAU Initialized OK");
}

void ProxyDecoderPrivate::deinitializeVdpau()
{
    if (m_vdpMixer != VDP_INVALID_HANDLE)
        vdp_video_mixer_destroy(m_vdpMixer);

    if (m_vdpDecoder != VDP_INVALID_HANDLE)
        vdp_decoder_destroy(m_vdpDecoder);

    for (int i = 0; i < m_renderStates.size(); ++i)
    {
        vdp_video_surface_destroy(m_renderStates[i]->surface);
        delete m_renderStates[i];
    }
}

void ProxyDecoderPrivate::initializeFfmpeg()
{
    AVCodec* codec = avcodec_find_decoder_by_name("h264_vdpau");
    if (!codec)
    {
        PRINT("avcodec_find_decoder_by_name(\"h264_vdpau\") failed");
        assert(false);
        return;
    }

    m_codecContext = avcodec_alloc_context3(codec);

    m_codecContext->opaque = this;

    m_codecContext->get_buffer2 = ProxyDecoderPrivate::callbackGetBuffer2;
    m_codecContext->draw_horiz_band = ProxyDecoderPrivate::callbackDrawHorizBand;
    m_codecContext->get_format = ProxyDecoderPrivate::callbackGetFormat;
    m_codecContext->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;

    int res = avcodec_open2(m_codecContext, codec, nullptr);
    if (res < 0)
        PRINT("avcodec_open2() failed with status " << res);
}

void ProxyDecoderPrivate::deinitializeFfmpeg()
{
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

int ProxyDecoderPrivate::callbackGetBuffer2(
    AVCodecContext* pContext, AVFrame* pFrame, int flags)
{
    LOG(":callbackGetBuffer2() BEGIN");
    ProxyDecoderPrivate* d = (ProxyDecoderPrivate*) pContext->opaque;
    assert(d);
    int result = d->doGetBuffer2(pContext, pFrame, flags);
    LOG(":callbackGetBuffer2() END");
    return result;
}

void ProxyDecoderPrivate::callbackDrawHorizBand(
    struct AVCodecContext* pContext, const AVFrame* src,
    int offset[4], int y, int type, int height)
{
    LOG(":callbackDrawHorizBand()");
    ProxyDecoderPrivate* d = (ProxyDecoderPrivate*) pContext->opaque;
    assert(d);
    d->doDrawHorizBand(pContext, src);
}

AVPixelFormat ProxyDecoderPrivate::callbackGetFormat(
    AVCodecContext* pContext, const AVPixelFormat* pFmt)
{
    LOG(":callbackGetFormat()");

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

int ProxyDecoderPrivate::doGetBuffer2(
    AVCodecContext* pContext, AVFrame* pFrame, int flags)
{
    vdpau_render_state* renderState = nullptr;
    for (int i = 0; i < m_renderStates.size(); ++i)
    {
        renderState = m_renderStates[i];
        if (!(renderState->state & FF_VDPAU_STATE_USED_FOR_REFERENCE))
        {
            LOG(":doGetBuffer2(): using vdpau_render_state[" << i << "]");
            break;
        }
    }

    if (!renderState) //< No free surfaces available - create new surface.
    {
        LOG(":doGetBuffer2(): creating new vdpau_render_state");

        renderState = new vdpau_render_state;
        m_renderStates.push_back(renderState);
        memset(renderState, 0, sizeof(vdpau_render_state));
        renderState->surface = VDP_INVALID_HANDLE;

        VDP(vdp_video_surface_create(getVdpDevice(),
            VDP_CHROMA_TYPE_420, m_frameWidth, m_frameHeight, &renderState->surface));
    }

    // TODO mike: Delete if not needed.
    pFrame->buf[0] = av_buffer_alloc(sizeof(renderState));

    pFrame->data[0] = (uint8_t*) renderState;
    //REMOVED: pFrame->type = FF_BUFFER_TYPE_USER;

    renderState->state |= FF_VDPAU_STATE_USED_FOR_REFERENCE;

    return 0;
}

void ProxyDecoderPrivate::doDrawHorizBand(AVCodecContext* pContext, const AVFrame* pFrame)
{
    assert(pFrame);
    vdpau_render_state* renderState = (vdpau_render_state*) pFrame->data[0];
    assert(renderState);

    assert(m_vdpDecoder != VDP_INVALID_HANDLE);

    TIME_BEGIN("vdp_decoder_render")
    VDP(vdp_decoder_render(m_vdpDecoder, renderState->surface,
        (VdpPictureInfo const*) &(renderState->info),
        renderState->bitstream_buffers_used, renderState->bitstream_buffers));
    TIME_END
}

void ProxyDecoderPrivate::fillAvPacket(AVPacket* packet,
    const ProxyDecoder::CompressedFrame* compressedFrame, uint64_t* pLastPts)
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
        static_assert(ProxyDecoder::CompressedFrame::kPaddingSize >= AV_INPUT_BUFFER_PADDING_SIZE,
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

    p[0] = b;
    p[1] = g;
    p[2] = r;
    p[3] = 0;
}

void ProxyDecoderPrivate::convertYuvToRgb(uint8_t* argbBuffer, int argbLineSize)
{
    // Software conversion of YUV to RGB.
    // Measured time for Full-HD frame:
    // 440 ms YUV FULL
    //  60 ms YUV PART
    //  86 ms Y_ONLY FULL
    //  12 ms Y_ONLY PART

    #ifdef ENABLE_RGB_PART_ONLY
        const int y0 = m_frameHeight / 3;
        const int yEnd = m_frameHeight * 2 / 3;
        const int x0 = m_frameWidth / 3;
        const int xEnd = m_frameWidth * 2 / 3;
    #else // ENABLE_RGB_PART_ONLY
        const int y0 = 0;
        const int yEnd = m_frameHeight;
        const int x0 = 0;
        const int xEnd = m_frameWidth;
    #endif // ENABLE_RGB_PART_ONLY

    uint8_t* pYSrc = m_yBufferStart + m_yLineSize * y0;
    uint8_t* pDestLine = argbBuffer + argbLineSize * y0;

    uint8_t* pVSrc = m_vBufferStart + m_uVLineSize * y0;
    uint8_t* pUSrc = m_uBufferStart + m_uVLineSize * y0;

    for (int y = y0; y < yEnd; ++y)
    {
        #ifdef ENABLE_RGB_Y_ONLY
            for (int x = x0; x < xEnd; ++x)
                ((uint32_t*) pDestLine)[x] = pYSrc[x]; //< Convert byte to 32-bit.
        #else // ENABLE_RGB_Y_ONLY
            for (int x = x0; x < xEnd; ++x)
                convertPixelYuvToRgb(pDestLine + x * 4, pYSrc[x], pUSrc[x / 2], pVSrc[x / 2]);
            if (y % 2 == 1)
            {
                pUSrc += m_uVLineSize;
                pVSrc += m_uVLineSize;
            }
        #endif // ENABLE_RGB_Y_ONLY

        pDestLine += argbLineSize;
        pYSrc += m_yLineSize;
    }
}

int ProxyDecoderPrivate::decodeFrame(
    const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
    vdpau_render_state** outRenderState)
{
    assert(outRenderState);

    AVPacket avpkt;
    fillAvPacket(&avpkt, compressedFrame, &m_lastPts);

    AVFrame* frame = av_frame_alloc();

    LOG("avcodec_decode_video2() BEGIN");
    int gotPicture = 0;
    int result;
    TIME_BEGIN("avcodec_decode_video2")
    result = avcodec_decode_video2(m_codecContext, frame, &gotPicture, &avpkt);
    TIME_END
    LOG("avcodec_decode_video2() END -> " << result << "; gotPicture: " << gotPicture << "");

    if (!gotPicture)
        result = 0;
    if (result <= 0) //< Negative result means error, zero means 'still buffering' - no picture.
    {
        av_frame_free(&frame);
        return result;
    }

    LOG(":decodeFrame(): frame size: " << frame->width << " x " << frame->height);

    *outRenderState = (vdpau_render_state*) frame->data[0];
    assert(*outRenderState);

    // Ffmpeg pts/dts are mixed up here, so it's pkt_dts. Also Convert us to ms.
    result = frame->coded_picture_number;
    if (outPts)
        *outPts = frame->pkt_dts / 1000;

    av_frame_free(&frame);

    return result;
}

int ProxyDecoderPrivate::decodeToRgb(
    const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    LOG("===========================================================================");
    LOG(":decodeToRgb(argbLineSize: " << argbLineSize << ") BEGIN");

    #ifdef ENABLE_RGB_VDPAU
        int result = decodeToRgbVdpau(compressedFrame, outPts, argbBuffer, argbLineSize);
    #else // ENABLE_RGB_VDPAU
        int result = decodeToRgbSoftware(compressedFrame, outPts, argbBuffer, argbLineSize);
    #endif // ENABLE_RGB_VDPAU

    LOG(":decodeToRgb() END -> " << result);
    return result;
}

int ProxyDecoderPrivate::decodeToRgbSoftware(
    const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    if (!m_yuvBuffersAllocated)
    {
        m_yuvBuffersAllocated = true;

        // TODO mike: Decide on alignment.
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
        m_yBufferStart, m_yLineSize, m_uBufferStart, m_vBufferStart, m_uVLineSize);

    if (result > 0)
    {
        TIME_BEGIN("convertYuvToRgb")
            convertYuvToRgb(argbBuffer, argbLineSize);
        TIME_END
    }

    return result;
}

int ProxyDecoderPrivate::decodeToRgbVdpau(
    const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPts, &renderState);
    if (result <= 0)
        return result;

    VdpOutputSurface outputSurface;
    VDP(vdp_output_surface_create(getVdpDevice(),
        VDP_RGBA_FORMAT_B8G8R8A8, m_frameWidth, m_frameHeight, &outputSurface));

    VDP(vdp_video_mixer_render(m_vdpMixer,
        /*background_surface*/ VDP_INVALID_HANDLE,
        /*background_source_rect*/ nullptr,
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
        /*video_surfaces_past_count*/ 0,
        /*video_surfaces_past*/ nullptr,
        renderState->surface,
        /*video_surfaces_future_count*/ 0,
        /*video_surfaces_future*/ nullptr,
        /*video_source_rect*/ nullptr,
        outputSurface,
        /*destination_rect*/ nullptr,
        /*destination_video_rect*/ nullptr,
        /*layer_count*/ 0,
        /*layers*/ nullptr));

    void* dataPtr = nullptr;
    size_t size = 0;
    void* const destinationData[2] = {&dataPtr, &size};

    // Nx extension to VDPAU: See libvdpau-sunxi::surface_output.c
    VDP(vdp_output_surface_get_bits_native(outputSurface, /*source_rect*/ nullptr,
        destinationData, /*destination_pitches*/ nullptr));

    memcpy(argbBuffer, dataPtr, size);

    VDP(vdp_output_surface_destroy(outputSurface));

    return result;
}

int ProxyDecoderPrivate::decodeToYuvPlanar(
    const ProxyDecoder::CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    LOG("===========================================================================");
    LOG(":decodeToYuvPlanar(yLineSize: " << yLineSize
        << ", uVLineSize: " << uVLineSize << ") BEGIN");

    vdpau_render_state* renderState;
    int result = decodeFrame(compressedFrame, outPts, &renderState);
    if (result <= 0)
        return result;

    assert(yBuffer);
    assert(vBuffer);
    assert(uBuffer);
    assert(yLineSize > 0);
    assert(uVLineSize > 0);

    void* const dest[3] = {yBuffer, vBuffer, uBuffer};
    const uint32_t pitches[3] = {
        (uint32_t) yLineSize, (uint32_t) uVLineSize, (uint32_t) uVLineSize};

#ifdef ENABLE_GET_BITS
    TIME_BEGIN("vdp_video_surface_get_bits_y_cb_cr")
    VDP(vdp_video_surface_get_bits_y_cb_cr(renderState->surface,
        VDP_YCBCR_FORMAT_YV12, dest, pitches));
    TIME_END
#endif // ENABLE_GET_BITS

    renderState->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE;

    LOG(":decodeToYuvPlanar() END -> " << result);
    return result;
}

//-------------------------------------------------------------------------------------------------
// ProxyDecoder

ProxyDecoder::ProxyDecoder(int frameWidth, int frameHeight)
:
    d(new ProxyDecoderPrivate(this, frameWidth, frameHeight))
{
}

ProxyDecoder::~ProxyDecoder()
{
}

int ProxyDecoder::decodeToRgb(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* argbBuffer, int argbLineSize)
{
    return d->decodeToRgb(compressedFrame, outPts, argbBuffer, argbLineSize);
}

int ProxyDecoder::decodeToYuvPlanar(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    return d->decodeToYuvPlanar(
        compressedFrame, outPts, yBuffer, yLineSize, uBuffer, vBuffer, uVLineSize);
}

int ProxyDecoder::decodeToYuvNative(const CompressedFrame* compressedFrame, int64_t* outPts,
    uint8_t** outBuffer, int* outBufferSize)
{
    // TODO mike: STUB
    return -113;
}

#endif // ENABLE_STUB
