// Uses configuration:
// #define ENABLE_X11
// #define ENABLE_LOG_VDPAU

// Uses:
// #define PRINT(...) ...

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

VdpGetProcAddress* vdp_get_proc_address;

VdpGetErrorString* vdp_get_error_string = NULL;

VdpVideoSurfaceGetParameters* vdp_video_surface_get_parameters;
VdpVideoSurfaceGetBitsYCbCr* vdp_video_surface_get_bits_y_cb_cr;
VdpVideoSurfaceCreate* vdp_video_surface_create;
VdpVideoSurfaceDestroy* vdp_video_surface_destroy;

VdpDeviceDestroy* vdp_device_destroy;

VdpDecoderCreate* vdp_decoder_create;
VdpDecoderDestroy* vdp_decoder_destroy;
VdpDecoderRender* vdp_decoder_render;

VdpOutputSurfaceCreate* vdp_output_surface_create;
VdpOutputSurfaceDestroy* vdp_output_surface_destroy;
VdpOutputSurfaceGetBitsNative* vdp_output_surface_get_bits_native;
VdpOutputSurfaceGetParameters* vdp_output_surface_get_parameters;

VdpVideoMixerCreate* vdp_video_mixer_create;
VdpVideoMixerDestroy* vdp_video_mixer_destroy;
VdpVideoMixerRender* vdp_video_mixer_render;

VdpPresentationQueueCreate* vdp_presentation_queue_create;
VdpPresentationQueueDestroy* vdp_presentation_queue_destroy;
VdpPresentationQueueGetTime* vdp_presentation_queue_get_time;
VdpPresentationQueueTargetCreateX11* vdp_presentation_queue_target_create_x11;
VdpPresentationQueueQuerySurfaceStatus* vdp_presentation_queue_query_surface_status;
VdpPresentationQueueDisplay* vdp_presentation_queue_display;
VdpPresentationQueueBlockUntilSurfaceIdle* vdp_presentation_queue_block_until_surface_idle;

void checkedVdpCall(VdpStatus status, const char* funcName)
{
    if (status != VDP_STATUS_OK)
    {
        if (vdp_get_error_string)
            PRINT("VDPAU ERROR: " << funcName << " -> '" << vdp_get_error_string(status) << "'");
        else
            PRINT("VDPAU ERROR: " << funcName << " -> " << status);
        assert(false);
    }
}

#ifdef ENABLE_LOG_VDPAU
    #define VDP(CALL) do \
    { \
        PRINT("VDPAU: " << #CALL); \
        checkedVdpCall(CALL, #CALL); \
    } while(0)
#else // ENABLE_LOG_VDPAU
    #define VDP(CALL) checkedVdpCall(CALL, #CALL)
#endif // ENABLE_LOG_VDPAU

VdpDevice createVdpDevice();

typedef struct
{
    void *virt; // user-space memory chunk with the existing surface bits data
    size_t size; // user-space memory chunk size
    uint32_t width;
    uint32_t height;
    VdpChromaType chroma_type;
    VdpYCbCrFormat source_format;
    int luma_size;
    int chroma_size;
} YuvNative;

/**
 * Nx extension to VDPAU vdp_video_surface_get_bits_y_cb_cr:
 * If destination_pitches is null, destinationData array is assumed to contain an array of
 * pointers to out params.
 */
void getVideoSurfaceYuvNative(VdpVideoSurface surface, YuvNative* yuvNative);

const char* vdpChromaTypeToStr(VdpChromaType v);
const char* vdpYCbCrFormatToStr(VdpYCbCrFormat v);

//-------------------------------------------------------------------------------------------------
// Implementation

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
        VDP_YCBCR_FORMAT_YV12, dest, /*pitches*/NULL));
}

void safeGetProcAddress(VdpDevice vdpDevice, VdpFuncId functionId, void** functionPointer)
{
    VDP(vdp_get_proc_address(vdpDevice, functionId, functionPointer));
    assert(*functionPointer);
}

static VdpDevice getVdpDevice()
{
    VdpDevice vdpDevice = VDP_INVALID_HANDLE;

    #ifdef ENABLE_X11

        Display* pXDisplay = /*NULL*/ XOpenDisplay(0);
        if (pXDisplay == NULL)
        {
            PRINT("ERROR: XOpenDisplay(0) returned null");
            return 0;
        }

        #if 0
            // Redirect stderr to /dev/null temporarily to prevent libvdpau console spam.
            int devNullFile = open("/dev/null", O_WRONLY);
            int stderrFile = dup(fileno(stderr));
            dup2(devNullFile, STDERR_FILENO);
        #endif

        PRINT("VDPAU: vdp_device_create_x11()...");
        VDP(vdp_device_create_x11(
            pXDisplay, /*0*/ DefaultScreen(pXDisplay), &vdpDevice, &vdp_get_proc_address));
        assert(vdp_get_proc_address);

        #if 0
            dup2(stderrFile, STDERR_FILENO);
            close(stderrFile);
        #endif

        PRINT("VDPAU: vdp_device_create_x11() OK");

    #else // ENABLE_X11

        PRINT("VDPAU: vdp_device_create_x11() without X11...");
        VDP(vdp_device_create_x11(NULL, -1, &vdpDevice, &vdp_get_proc_address));
        assert(vdp_get_proc_address);
        PRINT("VDPAU: vdp_device_create_x11() without X11 OK");

    #endif // ENABLE_X11

    return vdpDevice;
}

VdpDevice createVdpDevice()
{
    VdpDevice vdpDevice = getVdpDevice();

    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_GET_ERROR_STRING, (void**) &vdp_get_error_string);

    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_DEVICE_DESTROY, (void**)&vdp_device_destroy);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,
            (void**)&vdp_output_surface_create);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,
            (void**)&vdp_output_surface_destroy);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_GET_BITS_NATIVE,
            (void**)&vdp_output_surface_get_bits_native);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_CREATE, 
            (void**)&vdp_video_surface_create);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,
            (void**)&vdp_video_surface_destroy);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_DECODER_CREATE, (void**)&vdp_decoder_create);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_DECODER_DESTROY, (void**)&vdp_decoder_destroy);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_DECODER_RENDER, (void**)&vdp_decoder_render);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_GET_BITS_Y_CB_CR,
            (void**)&vdp_video_surface_get_bits_y_cb_cr);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_CREATE,
            (void**)&vdp_video_mixer_create);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_DESTROY,
            (void**)&vdp_video_mixer_destroy);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_MIXER_RENDER,
            (void**)&vdp_video_mixer_render);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE, 
            (void**)&vdp_presentation_queue_create);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY, 
            (void**)&vdp_presentation_queue_destroy);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
            (void**)&vdp_presentation_queue_target_create_x11);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_QUERY_SURFACE_STATUS, 
            (void**)&vdp_presentation_queue_query_surface_status);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,
            (void**)&vdp_presentation_queue_display);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME,
            (void**)&vdp_presentation_queue_get_time);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE, 
            (void**)&vdp_presentation_queue_block_until_surface_idle);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS,
            (void**)&vdp_video_surface_get_parameters);
    safeGetProcAddress(vdpDevice, VDP_FUNC_ID_OUTPUT_SURFACE_GET_PARAMETERS,
            (void**)&vdp_output_surface_get_parameters);

    return vdpDevice;
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
