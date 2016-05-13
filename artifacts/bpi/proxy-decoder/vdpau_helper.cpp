#include "vdpau_helper.h"

#include <stdio.h> //< For fileno
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

//-------------------------------------------------------------------------------------------------

VdpGetProcAddress* vdp_get_proc_address = NULL;
VdpGetErrorString* vdp_get_error_string = NULL;
VdpVideoSurfaceGetParameters* vdp_video_surface_get_parameters = NULL;
VdpVideoSurfaceGetBitsYCbCr* vdp_video_surface_get_bits_y_cb_cr = NULL;
VdpVideoSurfaceCreate* vdp_video_surface_create = NULL;
VdpVideoSurfaceDestroy* vdp_video_surface_destroy = NULL;
VdpDeviceDestroy* vdp_device_destroy = NULL;
VdpDecoderCreate* vdp_decoder_create = NULL;
VdpDecoderDestroy* vdp_decoder_destroy = NULL;
VdpDecoderRender* vdp_decoder_render = NULL;
VdpOutputSurfaceCreate* vdp_output_surface_create = NULL;
VdpOutputSurfaceDestroy* vdp_output_surface_destroy = NULL;
VdpOutputSurfaceGetBitsNative* vdp_output_surface_get_bits_native = NULL;
VdpOutputSurfaceGetParameters* vdp_output_surface_get_parameters = NULL;
VdpVideoMixerCreate* vdp_video_mixer_create = NULL;
VdpVideoMixerDestroy* vdp_video_mixer_destroy = NULL;
VdpVideoMixerRender* vdp_video_mixer_render = NULL;
VdpPresentationQueueCreate* vdp_presentation_queue_create = NULL;
VdpPresentationQueueDestroy* vdp_presentation_queue_destroy = NULL;
VdpPresentationQueueGetTime* vdp_presentation_queue_get_time = NULL;
VdpPresentationQueueTargetCreateX11* vdp_presentation_queue_target_create_x11 = NULL;
VdpPresentationQueueTargetDestroy* vdp_presentation_queue_target_destroy = NULL;
VdpPresentationQueueQuerySurfaceStatus* vdp_presentation_queue_query_surface_status = NULL;
VdpPresentationQueueDisplay* vdp_presentation_queue_display = NULL;
VdpPresentationQueueBlockUntilSurfaceIdle* vdp_presentation_queue_block_until_surface_idle = NULL;

//-------------------------------------------------------------------------------------------------

void checkedVdpCall(VdpStatus status, const char* funcName)
{
    if (status != VDP_STATUS_OK)
    {
        if (vdp_get_error_string)
            fprintf(stderr, "VDPAU ERROR: %s -> '%s'\n", funcName, vdp_get_error_string(status));
        else
            fprintf(stderr, "VDPAU ERROR: %s -> %d\n", funcName, status);
        assert(false);
    }
}

void vdpCheckHandle(uint32_t handle, const char* name)
{
    if (handle == VDP_INVALID_HANDLE)
    {
        fprintf(stderr, "VDPAU ERROR: Unable to create %s.\n", name);
        assert(false);
    }
}

static VdpDevice getVdpDeviceX11()
{
    fprintf(stderr, "VDPAU: Opening X11 Display...\n");
    Display *pXDisplay = XOpenDisplay(0);
    if (pXDisplay == NULL)
    {
        fprintf(stderr, "VDPAU ERROR: XOpenDisplay(0) returned null.\n");
        assert(false);
    }

    int stderrFile = -1;
    if (suppressX11LogVdpau())
    {
        // Redirect stderr to /dev/null temporarily to prevent libvdpau console spam.
        int devNullFile = open("/dev/null", O_WRONLY);
        stderrFile = dup(fileno(stderr));
        dup2(devNullFile, STDERR_FILENO);
    }

    fprintf(stderr, "VDPAU: vdp_device_create_x11() with X11...\n");
    VdpDevice vdpDevice = VDP_INVALID_HANDLE;
    VDP(vdp_device_create_x11(
        pXDisplay,  DefaultScreen(pXDisplay), &vdpDevice, &vdp_get_proc_address));

    if (suppressX11LogVdpau())
    {
        dup2(stderrFile, STDERR_FILENO);
        close(stderrFile);
    }

    vdpCheckHandle(vdpDevice, "Device");
    assert(vdp_get_proc_address);

    fprintf(stderr, "VDPAU: vdp_device_create_x11() with X11 OK\n");
    return vdpDevice;
}

static VdpDevice getVdpDeviceWithoutX11()
{
    fprintf(stderr, "VDPAU: vdp_device_create_x11() without X11 (Nx extension to VDPAU)...\n");
    VdpDevice vdpDevice = VDP_INVALID_HANDLE;
    VDP(vdp_device_create_x11(NULL, -1, &vdpDevice, &vdp_get_proc_address));

    vdpCheckHandle(vdpDevice, "Device");
    assert(vdp_get_proc_address);

    fprintf(stderr, "VDPAU: vdp_device_create_x11() without X11 OK\n");
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

VdpDevice createVdpDevice()
{
    VdpDevice vdpDevice = enableX11Vdpau() ?  getVdpDeviceX11() : getVdpDeviceWithoutX11();
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
        VDP_YCBCR_FORMAT_YV12, dest, /*pitches*/ NULL));
}

static void logStrOrHex(
    const char *prefix, const char *valueStr, uint32_t value, const char *suffix)
{
    if (valueStr)
        fprintf(stderr, "%s%s%s", prefix, valueStr, suffix);
    else
        fprintf(stderr, "%s0x%08X%s", prefix, value, suffix);
}

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
    fprintf(stderr, "YuvNative { ");
    fprintf(stderr, "virt %p, size %u, ", y->virt, (unsigned int) y->size);
    fprintf(stderr, "width %d, height %d, ", y->width, y->height);
    logStrOrHex("chroma_type ", vdpChromaTypeToStr(y->chroma_type), y->chroma_type, ", ");
    logStrOrHex("source_format ", vdpYCbCrFormatToStr(y->source_format), y->source_format, ", ");
    fprintf(stderr, "luma_size %d, chroma_size %d }\n", y->luma_size, y->chroma_size);
}

const char* vdpChromaTypeToStr(VdpChromaType v)
{
    switch (v)
    {
        case VDP_CHROMA_TYPE_420: return "VDP_CHROMA_TYPE_420";
        case VDP_CHROMA_TYPE_422: return "VDP_CHROMA_TYPE_422";
        case VDP_CHROMA_TYPE_444: return "VDP_CHROMA_TYPE_444";
        default: return NULL;
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
        default: return NULL;
    }
}

static FILE* debugCreateFrameDumpFile(
    const char *filesPath, int w, int h, int n, const char *suffix, const char *mode)
{
    char filename[1000];
    snprintf(filename, sizeof(filename), "%s/frame_%dx%d_%d%s", filesPath, w, h, n, suffix);
    FILE* fp = fopen(filename, mode);
    if (fp == NULL)
    {
        fprintf(stderr, "VDPAU DUMP ERROR: Unable to create file: %s\n", filename);
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
        fprintf(stderr, "VDPAU DUMP ERROR: Unable to write to a file.\n");
        assert(false);
    }
    fclose(fp);
}

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

    fprintf(stderr, "VDPAU DUMP: Frame %d dumped to %s\n", n, filesPath);
}
