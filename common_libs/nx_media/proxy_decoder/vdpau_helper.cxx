// TODO mike: Transform into a unit.

VdpGetProcAddress* vdp_get_proc_address;

VdpGetErrorString* vdp_get_error_string = nullptr;

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

static void checkedVdpCall(VdpStatus status, const char* funcName)
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

VdpDevice getVdpDevice();

//-------------------------------------------------------------------------------------------------
// Implementation

void safeGetProcAddress(VdpFuncId functionId, void** functionPointer)
{
    VDP(vdp_get_proc_address(getVdpDevice(), functionId, functionPointer));
    assert(*functionPointer);
}

VdpDevice getVdpDevice()
{
    static VdpDevice vdpDevice = 0;
    static bool bInitFailed = false;

    if (vdpDevice) {
        return vdpDevice;
    }

    if (bInitFailed) {
        return 0;
    }

    Display* pXDisplay = /*nullptr*/ XOpenDisplay(0);
    if (pXDisplay == nullptr)
    {
        PRINT("ERROR: XOpenDisplay(0) returned null");
        bInitFailed = true;
        return 0;
    }

    VdpStatus status;

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

    safeGetProcAddress(VDP_FUNC_ID_GET_ERROR_STRING, (void**) &vdp_get_error_string);

    safeGetProcAddress(VDP_FUNC_ID_DEVICE_DESTROY, (void**)&vdp_device_destroy);
    safeGetProcAddress(VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,
            (void**)&vdp_output_surface_create);
    safeGetProcAddress(VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,
            (void**)&vdp_output_surface_destroy);
    safeGetProcAddress(VDP_FUNC_ID_OUTPUT_SURFACE_GET_BITS_NATIVE,
            (void**)&vdp_output_surface_get_bits_native);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_SURFACE_CREATE, 
            (void**)&vdp_video_surface_create);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,
            (void**)&vdp_video_surface_destroy);
    safeGetProcAddress(VDP_FUNC_ID_DECODER_CREATE, (void**)&vdp_decoder_create);
    safeGetProcAddress(VDP_FUNC_ID_DECODER_DESTROY, (void**)&vdp_decoder_destroy);
    safeGetProcAddress(VDP_FUNC_ID_DECODER_RENDER, (void**)&vdp_decoder_render);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_SURFACE_GET_BITS_Y_CB_CR,
            (void**)&vdp_video_surface_get_bits_y_cb_cr);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_MIXER_CREATE,
            (void**)&vdp_video_mixer_create);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_MIXER_DESTROY,
            (void**)&vdp_video_mixer_destroy);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_MIXER_RENDER,
            (void**)&vdp_video_mixer_render);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE, 
            (void**)&vdp_presentation_queue_create);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY, 
            (void**)&vdp_presentation_queue_destroy);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
            (void**)&vdp_presentation_queue_target_create_x11);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_QUERY_SURFACE_STATUS, 
            (void**)&vdp_presentation_queue_query_surface_status);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,
            (void**)&vdp_presentation_queue_display);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME,
            (void**)&vdp_presentation_queue_get_time);
    safeGetProcAddress(VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE, 
            (void**)&vdp_presentation_queue_block_until_surface_idle);
    safeGetProcAddress(VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS,
            (void**)&vdp_video_surface_get_parameters);
    safeGetProcAddress(VDP_FUNC_ID_OUTPUT_SURFACE_GET_PARAMETERS,
            (void**)&vdp_output_surface_get_parameters);

    return vdpDevice;
}
