#include "proxy_decoder.h"

// Alternative impl of ProxyDecoder as a pure software stub which just draws a checkerboard.

#include <cassert>
#include <cstring>

#include "vdpau_helper.h"

#define OUTPUT_PREFIX "proxydecoder[STUB]: "
#include "proxy_decoder_utils.h"

namespace {

//-------------------------------------------------------------------------------------------------

/**
 * Used to implement decodeToDisplayQueue() / displayDecoded() as Y-checkerboard.
 */
class VdpauStub
{
public:
    VdpauStub(int frameWidth, int frameHeight);
    ~VdpauStub();
    void display();

private:
    int m_frameWidth;
    int m_frameHeight;

    VdpDevice m_vdpDevice = VDP_INVALID_HANDLE;
    VdpDecoder m_vdpDecoder = VDP_INVALID_HANDLE;
    VdpVideoMixer m_vdpMixer = VDP_INVALID_HANDLE;
    VdpPresentationQueueTarget m_vdpTarget = VDP_INVALID_HANDLE;
    VdpPresentationQueue m_vdpQueue = VDP_INVALID_HANDLE;

    int m_outputSurfaceIndex = 0;
    std::vector<VdpOutputSurface> m_outputSurfaces;

    int m_videoSurfaceIndex = 0;
    std::vector<VdpVideoSurface> m_vdpVideoSurfaces;
};

VdpauStub::VdpauStub(int frameWidth, int frameHeight)
:
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight)
{
    if (conf.videoSurfaceCount < 1 || conf.videoSurfaceCount > 16)
    {
        PRINT << "WARNING: configuration param videoSurfaceCount is "
            << conf.videoSurfaceCount << " but should be 1..16; defaults to 1.";
        conf.videoSurfaceCount = 1;
    }
    if (conf.outputSurfaceCount < 0 || conf.outputSurfaceCount > 255)
    {
        PRINT << "WARNING: configuration param outputSurfaceCount is "
            << conf.outputSurfaceCount << " but should be 1..100; defaults to 1.";
        conf.outputSurfaceCount = 1;
    }
    PRINT << "Initializing VDPAU; using "
        << conf.videoSurfaceCount << " video surfaces, "
        << conf.outputSurfaceCount << " output surfaces";

    Drawable drawable;
    m_vdpDevice = createVdpDevice(&drawable);

    VDP(vdp_decoder_create(m_vdpDevice,
        VDP_DECODER_PROFILE_H264_HIGH,
        m_frameWidth,
        m_frameHeight,
        /*max_references*/ conf.videoSurfaceCount, //< Used only for checking to be <= 16.
        &m_vdpDecoder));
    vdpCheckHandle(m_vdpDecoder, "Decoder");

    VDP(vdp_video_mixer_create(m_vdpDevice,
        /*feature_count*/ 0,
        /*features*/ nullptr,
        /*parameter_count*/ 0,
        /*parameters*/ nullptr,
        /*parameter_values*/ nullptr,
        &m_vdpMixer));
    vdpCheckHandle(m_vdpMixer, "Mixer");

    if (!conf.disableCscMatrix)
    {
        static const VdpVideoMixerAttribute attr = VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX;
        // Values grabbed from mpv logs.
        static const VdpCSCMatrix matrix =
            {
                {1.164384, 0.000000, 1.792741, -0.972945},
                {1.164384, -0.213249, -0.532909, 0.301483},
                {1.164384, 2.112402, 0.000000, -1.133402},
            };
        static const VdpCSCMatrix* const pMatrix = &matrix;

        VDP(vdp_video_mixer_set_attribute_values(m_vdpMixer, /*attributes_count*/ 1, &attr,
            (void const * const *) &pMatrix));
    }

    VDP(vdp_presentation_queue_target_create_x11(m_vdpDevice, drawable, &m_vdpTarget));
    vdpCheckHandle(m_vdpTarget, "Presentation Queue Target");

    VDP(vdp_presentation_queue_create(m_vdpDevice, m_vdpTarget, &m_vdpQueue));
    vdpCheckHandle(m_vdpQueue, "Presentation Queue");

    while (m_vdpVideoSurfaces.size() < conf.videoSurfaceCount)
    {
        VdpVideoSurface surface = VDP_INVALID_HANDLE;
        VDP(vdp_video_surface_create(m_vdpDevice,
            VDP_CHROMA_TYPE_420, m_frameWidth, m_frameHeight, &surface));
        vdpCheckHandle(surface, "Video Surface");

        m_vdpVideoSurfaces.push_back(surface);

        static YuvNative yuvNative;
        getVideoSurfaceYuvNative(surface, &yuvNative);
        memset(yuvNative.virt, 0, yuvNative.size);
    }

    for (int i = 0; i < conf.outputSurfaceCount; ++i)
    {
        VdpOutputSurface outputSurface = VDP_INVALID_HANDLE;
        VDP(vdp_output_surface_create(m_vdpDevice,
            VDP_RGBA_FORMAT_B8G8R8A8, m_frameWidth, m_frameHeight, &outputSurface));
        vdpCheckHandle(outputSurface, "Output Surface");
        m_outputSurfaces.push_back(outputSurface);
    }

    PRINT << "VDPAU Initialized OK";
}

VdpauStub::~VdpauStub()
{
    OUTPUT << "Deinitializing VDPAU BEGIN";

    for (auto& surface: m_vdpVideoSurfaces)
    {
        if (surface != VDP_INVALID_HANDLE)
            VDP(vdp_video_surface_destroy(surface));
        surface = VDP_INVALID_HANDLE;
    }

    if (m_vdpQueue != VDP_INVALID_HANDLE)
        VDP(vdp_presentation_queue_destroy(m_vdpQueue));

    if (m_vdpTarget != VDP_INVALID_HANDLE)
        VDP(vdp_presentation_queue_target_destroy(m_vdpTarget));

    if (m_vdpMixer != VDP_INVALID_HANDLE)
        VDP(vdp_video_mixer_destroy(m_vdpMixer));

    if (m_vdpDecoder != VDP_INVALID_HANDLE)
        VDP(vdp_decoder_destroy(m_vdpDecoder));

    if (m_vdpDevice != VDP_INVALID_HANDLE)
        VDP(vdp_device_destroy(m_vdpDevice));

    OUTPUT << "Deinitializing VDPAU END";
}

void VdpauStub::display()
{
    const int videoSurfaceIndex = m_videoSurfaceIndex;
    m_videoSurfaceIndex = (m_videoSurfaceIndex + 1) % conf.videoSurfaceCount;

    VdpVideoSurface videoSurface = m_vdpVideoSurfaces[videoSurfaceIndex];
    assert(videoSurface != VDP_INVALID_HANDLE);

    OUTPUT << "VdpauStub::display() BEGIN";
    OUTPUT << stringFormat("Using videoSurface %02d of %d {handle #%02d}",
        videoSurfaceIndex, conf.videoSurfaceCount, videoSurface);

    static YuvNative yuvNative;
    getVideoSurfaceYuvNative(videoSurface, &yuvNative);
    memset(yuvNative.virt, 0, yuvNative.luma_size);
    //debugDrawCheckerboardYNative((uint8_t*) yuvNative.virt, m_frameWidth, m_frameHeight);
    // Print videoSurfaceIndex in its individual position.
    debugPrintNative((uint8_t*) yuvNative.virt, m_frameWidth, m_frameHeight,
        /*x*/ 12 * (videoSurfaceIndex % 4), /*y*/ 6 * (videoSurfaceIndex / 4),
        stringFormat("%02d", videoSurface).c_str());

    VdpOutputSurface outputSurface = VDP_INVALID_HANDLE;
    if (conf.outputSurfaceCount == 0)
    {
        VDP(vdp_output_surface_create(m_vdpDevice,
            VDP_RGBA_FORMAT_B8G8R8A8, m_frameWidth, m_frameHeight, &outputSurface));
        vdpCheckHandle(outputSurface, "Output Surface");
    }
    else
    {
        const int outputSurfaceIndex = m_outputSurfaceIndex;
        m_outputSurfaceIndex = (m_outputSurfaceIndex + 1) % conf.outputSurfaceCount;
        outputSurface = m_outputSurfaces[outputSurfaceIndex];
        OUTPUT << stringFormat("Using outputSurface %02d of %d {handle #%02d}",
                outputSurfaceIndex, conf.outputSurfaceCount, outputSurface);
    }
    assert(outputSurface != VDP_INVALID_HANDLE);

    // In vdpau_sunxi, this links Video Surface to Output Surface; does not process pixel data.
    VDP(vdp_video_mixer_render(m_vdpMixer,
        /*background_surface*/ VDP_INVALID_HANDLE, //< Not implemented.
        /*background_source_rect*/ nullptr, //< Not implemented.
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME, //< Other values not implemented.
        /*video_surfaces_past_count*/ 0, //< Not implemented.
        /*video_surfaces_past*/ nullptr, //< Not implemented.
        /*video_surface_current*/ videoSurface,
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

    if (conf.outputSurfaceCount == 0)
        VDP(vdp_output_surface_destroy(outputSurface));

    OUTPUT << "VdpauStub::display() END";
}

//-------------------------------------------------------------------------------------------------

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

    std::unique_ptr<VdpauStub> m_vdpauStub;
};

Stub::Stub(int frameWidth, int frameHeight)
:
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight),
    m_frameNumber(0)
{
    assert(frameWidth > 0);
    assert(frameHeight > 0);

    OUTPUT << "Stub(frameWidth: " << frameWidth << ", frameHeight: " << frameHeight << ")";

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
        OUTPUT << "decodeToRgb(argbLineSize: " << argbLineSize << ")";
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
        OUTPUT << "decodeToYuvPlanar(yLineSize: " << yLineSize
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
    OUTPUT << "decodeToDisplayQueue()";

    assert(outFrameHandle);
    *outFrameHandle = (void*) (1);

    if (outPts && compressedFrame)
        *outPts = compressedFrame->pts;
    return ++m_frameNumber;
}

void Stub::displayDecoded(void* handle)
{
    assert(handle == (void*) (1));

    if (!m_vdpauStub)
        m_vdpauStub.reset(new VdpauStub(m_frameWidth, m_frameHeight));

    if (conf.enableFps)
        debugShowFps(OUTPUT_PREFIX);

    m_vdpauStub->display();
}

} // namespace

//-------------------------------------------------------------------------------------------------

ProxyDecoder* ProxyDecoder::createStub(int frameWidth, int frameHeight)
{
    conf.reload();
    return new Stub(frameWidth, frameHeight);
}
