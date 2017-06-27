#include "vdpau_utils.h"

#include <stdio.h> //< For fileno
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "proxy_decoder_utils.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "proxydecoder[vdpau]: "

//-------------------------------------------------------------------------------------------------

uint32_t g_fullScreenWidthHeight = 0;

VdpGetProcAddress* vdp_get_proc_address = nullptr;
VdpGetErrorString* vdp_get_error_string = nullptr;
VdpVideoSurfaceGetParameters* vdp_video_surface_get_parameters = nullptr;
VdpVideoSurfaceGetBitsYCbCr* vdp_video_surface_get_bits_y_cb_cr = nullptr;
VdpVideoSurfaceCreate* vdp_video_surface_create = nullptr;
VdpVideoSurfaceDestroy* vdp_video_surface_destroy = nullptr;
VdpDeviceDestroy* vdp_device_destroy = nullptr;
VdpDecoderCreate* vdp_decoder_create = nullptr;
VdpDecoderDestroy* vdp_decoder_destroy = nullptr;
VdpDecoderRender* vdp_decoder_render = nullptr;
VdpOutputSurfaceCreate* vdp_output_surface_create = nullptr;
VdpOutputSurfaceDestroy* vdp_output_surface_destroy = nullptr;
VdpOutputSurfaceGetBitsNative* vdp_output_surface_get_bits_native = nullptr;
VdpOutputSurfaceGetParameters* vdp_output_surface_get_parameters = nullptr;
VdpVideoMixerCreate* vdp_video_mixer_create = nullptr;
VdpVideoMixerDestroy* vdp_video_mixer_destroy = nullptr;
VdpVideoMixerSetAttributeValues* vdp_video_mixer_set_attribute_values = nullptr;
VdpVideoMixerRender* vdp_video_mixer_render = nullptr;
VdpPresentationQueueCreate* vdp_presentation_queue_create = nullptr;
VdpPresentationQueueDestroy* vdp_presentation_queue_destroy = nullptr;
VdpPresentationQueueGetTime* vdp_presentation_queue_get_time = nullptr;
VdpPresentationQueueTargetCreateX11* vdp_presentation_queue_target_create_x11 = nullptr;
VdpPresentationQueueTargetDestroy* vdp_presentation_queue_target_destroy = nullptr;
VdpPresentationQueueQuerySurfaceStatus* vdp_presentation_queue_query_surface_status = nullptr;
VdpPresentationQueueDisplay* vdp_presentation_queue_display = nullptr;
VdpPresentationQueueBlockUntilSurfaceIdle* vdp_presentation_queue_block_until_surface_idle = nullptr;

//-------------------------------------------------------------------------------------------------

void checkedVdpCall(VdpStatus status, const char* funcName)
{
    if (status != VDP_STATUS_OK)
    {
        if (vdp_get_error_string)
            NX_PRINT << "VDPAU ERROR: " << funcName << " -> '" << vdp_get_error_string(status) << "'";
        else
            NX_PRINT << "VDPAU ERROR: " << funcName << " -> " << status;
        assert(false);
    }
}

void vdpCheckHandle(uint32_t handle, const char* name)
{
    if (handle == VDP_INVALID_HANDLE)
    {
        NX_PRINT << "VDPAU ERROR: Unable to create " << name << ".";
        assert(false);
    }
}

namespace {

static Drawable createX11Window(Display* display)
{
    const int screen = DefaultScreen(display);
    const unsigned long black = BlackPixel(display, screen);
    const unsigned long white = WhitePixel(display, screen);

    Window rootWindow = RootWindow(display, screen);

    XWindowAttributes rootWindowAttrs;
    if (!XGetWindowAttributes(display, rootWindow, &rootWindowAttrs))
    {
        NX_PRINT << "ERROR: XGetWindowAttributes() for Root Window failed.";
        assert(false);
    }

    const Window window = XCreateSimpleWindow(
        display, rootWindow, /*x*/ 0, /*y*/ 0, rootWindowAttrs.width, rootWindowAttrs.height,
        /*border_width*/ 0, black, white);
    g_fullScreenWidthHeight = (rootWindowAttrs.height << 16) | rootWindowAttrs.width;

    if (!XMapWindow(display, window))
    {
        NX_PRINT << "ERROR: XMapWindow() failed.";
        assert(false);
    }

    if (!XFlush(display))
    {
        NX_PRINT << "ERROR: XFlush() failed.";
        assert(false);
    }

    return window;
}

static VdpDevice getVdpDeviceX11(Drawable* outDrawable)
{
    assert(outDrawable);

    NX_PRINT << "Opening X11 Display...";
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        NX_PRINT << "ERROR: XOpenDisplay(nullptr) returned null.";
        assert(false);
    }

    int stderrFile = -1;
    if (ini().suppressX11LogVdpau)
    {
        // Redirect stderr to /dev/null temporarily to prevent libvdpau console spam.
        int devNullFile = open("/dev/null", O_WRONLY);
        stderrFile = dup(fileno(stderr));
        dup2(devNullFile, STDERR_FILENO);
    }

    NX_PRINT << "vdp_device_create_x11() with X11...";
    VdpDevice vdpDevice = VDP_INVALID_HANDLE;
    VDP(vdp_device_create_x11(
        display,  DefaultScreen(display), &vdpDevice, &vdp_get_proc_address));

    if (ini().suppressX11LogVdpau)
    {
        dup2(stderrFile, STDERR_FILENO);
        close(stderrFile);
    }

    vdpCheckHandle(vdpDevice, "Device");
    assert(vdp_get_proc_address);

    if (ini().createX11Window)
        *outDrawable = createX11Window(display);
    else
        *outDrawable = 0;

    NX_PRINT << "vdp_device_create_x11() with X11 OK";
    return vdpDevice;
}

static VdpDevice getVdpDeviceWithoutX11(Drawable* outDrawable)
{
    NX_PRINT << "vdp_device_create_x11() without X11 (Nx extension to VDPAU)...";
    assert(outDrawable);
    *outDrawable = 0;
    VdpDevice vdpDevice = VDP_INVALID_HANDLE;

    VDP(vdp_device_create_x11(
        (Display*) &g_fullScreenWidthHeight, -1, &vdpDevice, &vdp_get_proc_address));
    vdpCheckHandle(vdpDevice, "Device");
    assert(vdp_get_proc_address);

    NX_PRINT << "vdp_device_create_x11() without X11 OK";
    return vdpDevice;
}

static void getProcAddress(VdpDevice vdpDevice, VdpFuncId functionId, void **functionPointer)
{
    VDP(vdp_get_proc_address(vdpDevice, functionId, functionPointer));
    assert(*functionPointer);
}

static void getProcAddresses(VdpDevice vdpDevice)
{
    getProcAddress(vdpDevice, VDP_FUNC_ID_GET_ERROR_STRING,
        (void**) &vdp_get_error_string);
    getProcAddress(vdpDevice, VDP_FUNC_ID_DEVICE_DESTROY,
        (void**) &vdp_device_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,
        (void**) &vdp_output_surface_create);
    getProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,
        (void**) &vdp_output_surface_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_GET_BITS_NATIVE,
        (void**) &vdp_output_surface_get_bits_native);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_CREATE,
        (void**) &vdp_video_surface_create);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,
        (void**) &vdp_video_surface_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_DECODER_CREATE,
        (void**) &vdp_decoder_create);
    getProcAddress(vdpDevice, VDP_FUNC_ID_DECODER_DESTROY,
        (void**) &vdp_decoder_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_DECODER_RENDER,
        (void**) &vdp_decoder_render);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_GET_BITS_Y_CB_CR,
        (void**) &vdp_video_surface_get_bits_y_cb_cr);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_CREATE,
        (void**) &vdp_video_mixer_create);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_DESTROY,
        (void**) &vdp_video_mixer_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES,
        (void**) &vdp_video_mixer_set_attribute_values);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_RENDER,
        (void**) &vdp_video_mixer_render);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE,
        (void**) &vdp_presentation_queue_create);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY,
        (void**) &vdp_presentation_queue_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
        (void**) &vdp_presentation_queue_target_create_x11);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY,
        (void**) &vdp_presentation_queue_target_destroy);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_QUERY_SURFACE_STATUS,
        (void**) &vdp_presentation_queue_query_surface_status);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,
        (void**) &vdp_presentation_queue_display);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME,
        (void**) &vdp_presentation_queue_get_time);
    getProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE,
        (void**) &vdp_presentation_queue_block_until_surface_idle);
    getProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS,
        (void**) &vdp_video_surface_get_parameters);
    getProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_GET_PARAMETERS,
        (void**) &vdp_output_surface_get_parameters);
}

} // namespace

VdpDevice createVdpDevice(Drawable* outDrawable)
{
    VdpDevice vdpDevice =
        ini().enableX11Vdpau ? getVdpDeviceX11(outDrawable) : getVdpDeviceWithoutX11(outDrawable);
    getProcAddresses(vdpDevice);
    return vdpDevice;
}

//-------------------------------------------------------------------------------------------------

void getVideoSurfaceYuvNative(VdpVideoSurface surface, YuvNative* yuvNative)
{
    void* const dest[] =
    {
        &yuvNative->virt,
        &yuvNative->size,
        &yuvNative->width,
        &yuvNative->height,
        &yuvNative->chroma_type,
        &yuvNative->source_format,
        &yuvNative->luma_size,
        &yuvNative->chroma_size,
    };

    VDP(vdp_video_surface_get_bits_y_cb_cr(surface,
        VDP_YCBCR_FORMAT_YV12, dest, /*pitches*/ nullptr));
}

namespace {

static std::string toStrOrHex(const char* valueStr, uint32_t value)
{
    if (valueStr)
        return valueStr;
    else
        return format("0x%08X", value);
}

} // namespace

uint32_t calcYuvNativeQuickHash(const YuvNative* yuvNative)
{
    static const int dwordsCount = 256; //< Limit of data being scanned.

    uint32_t hash = 0;
    for (int i = 0; i < dwordsCount; ++i)
        hash ^= ((const uint32_t*) yuvNative->virt)[i * 256];

    return hash;
}

void logYuvNative(const YuvNative* y)
{
    NX_PRINT << "YuvNative { "
        << "virt " << format("%p", y->virt)
        << ", size " << y->size << ", width " << y->width << ", height " << y->height
        << ", chroma_type " << toStrOrHex(vdpChromaTypeToStr(y->chroma_type), y->chroma_type)
        << ", source_format " << toStrOrHex(vdpYCbCrFormatToStr(y->source_format), y->source_format)
        << ", luma_size " << y->luma_size << ", chroma_size " << y->chroma_size
        << " }";
}

const char* vdpChromaTypeToStr(VdpChromaType v)
{
    switch (v)
    {
        case VDP_CHROMA_TYPE_420: return "VDP_CHROMA_TYPE_420";
        case VDP_CHROMA_TYPE_422: return "VDP_CHROMA_TYPE_422";
        case VDP_CHROMA_TYPE_444: return "VDP_CHROMA_TYPE_444";
        default: return nullptr;
    }
}

const char* vdpYCbCrFormatToStr(VdpYCbCrFormat v)
{
    switch (v)
    {
        case VDP_YCBCR_FORMAT_NV12: return "VDP_YCBCR_FORMAT_NV12";
        case VDP_YCBCR_FORMAT_YV12: return "VDP_YCBCR_FORMAT_YV12";
        case VDP_YCBCR_FORMAT_UYVY: return "VDP_YCBCR_FORMAT_UYVY";
        case VDP_YCBCR_FORMAT_YUYV: return "VDP_YCBCR_FORMAT_YUYV";
        case VDP_YCBCR_FORMAT_Y8U8V8A8: return "VDP_YCBCR_FORMAT_Y8U8V8A8";
        case VDP_YCBCR_FORMAT_V8U8Y8A8: return "VDP_YCBCR_FORMAT_V8U8Y8A8";
        default: return nullptr;
    }
}

namespace {

static FILE* debugCreateFrameDumpFile(
    const char *filesPath, int w, int h, int n, const char *suffix, const char *mode)
{
    char filename[1000];
    snprintf(filename, sizeof(filename), "%s/frame_%dx%d_%d%s", filesPath, w, h, n, suffix);
    FILE* fp = fopen(filename, mode);
    if (fp == nullptr)
    {
        NX_PRINT << "ERROR: Unable to create dump file: " << filename;
        assert(false);
    }
    return fp;
}

static void debugDumpFrameToFile(
    const char *filesPath, void* buffer, int bufferSize, int w, int h, int n, const char* suffix)
{
    FILE* fp = debugCreateFrameDumpFile(filesPath, w, h, n, suffix, "wb");
    if (fwrite(buffer, 1, bufferSize, fp) != bufferSize)
    {
        NX_PRINT << "ERROR: Unable to write to a dump file.";
        assert(false);
    }
    fclose(fp);
}

} // namespace

void debugDumpYuvSurfaceToFiles(
    const char *filesPath, VdpVideoSurface surface,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    static int n = 0;
    ++n;

    YuvNative yuvNative;
    getVideoSurfaceYuvNative(surface, &yuvNative);

    const int w = yuvNative.width;
    const int h = yuvNative.height;

    FILE* fp = debugCreateFrameDumpFile(filesPath, w, h, n, ".txt", "w");

    fprintf(fp, "n=%d\n", n);
    fprintf(fp, "yLineSize=%d\n", yLineSize);
    fprintf(fp, "uVLineSize=%d\n", uVLineSize);
    fprintf(fp, "YuvNative.width=%d\n", yuvNative.width);
    fprintf(fp, "YuvNative.height=%d\n", yuvNative.height);

    const char* chromaTypeS = vdpChromaTypeToStr(yuvNative.chroma_type);
    if (chromaTypeS)
        fprintf(fp, "YuvNative.chroma_type=%s\n", chromaTypeS);
    else
        fprintf(fp, "YuvNative.chroma_type=%d\n", yuvNative.chroma_type);

    const char *sourceFormatS = vdpYCbCrFormatToStr(yuvNative.source_format);
    if (sourceFormatS)
        fprintf(fp, "YuvNative.source_format=%s\n", sourceFormatS);
    else
        fprintf(fp, "YuvNative.source_format=%d\n", yuvNative.source_format);

    fprintf(fp, "YuvNative.luma_size=%d\n", yuvNative.luma_size);
    fprintf(fp, "YuvNative.chroma_size=%d\n", yuvNative.chroma_size);

    fclose(fp);

    debugDumpFrameToFile(filesPath, yBuffer, yLineSize * h, w, h, n, "_y.dat");
    debugDumpFrameToFile(filesPath, uBuffer, uVLineSize * h / 2, w, h, n, "_u.dat");
    debugDumpFrameToFile(filesPath, vBuffer, uVLineSize * h / 2, w, h, n, "_v.dat");
    debugDumpFrameToFile(filesPath, yuvNative.virt, (int) yuvNative.size, w, h, n, "_native.dat");

    NX_PRINT << "Dump: Frame " << n << " dumped to " << filesPath;
}
