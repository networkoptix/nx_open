#pragma once

#include <stdio.h>
#include <stdint.h>

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

// If run without X11, filled after calling vdp_presentation_queue_target_create_x11().
extern uint32_t g_fullScreenWidthHeight; //< (height << 16) | width

// VDPAU function pointers filled by createVdpDevice().
extern VdpGetProcAddress* vdp_get_proc_address;
extern VdpGetErrorString* vdp_get_error_string;
extern VdpVideoSurfaceGetParameters* vdp_video_surface_get_parameters;
extern VdpVideoSurfaceGetBitsYCbCr* vdp_video_surface_get_bits_y_cb_cr;
extern VdpVideoSurfaceCreate* vdp_video_surface_create;
extern VdpVideoSurfaceDestroy* vdp_video_surface_destroy;
extern VdpDeviceDestroy* vdp_device_destroy;
extern VdpDecoderCreate* vdp_decoder_create;
extern VdpDecoderDestroy* vdp_decoder_destroy;
extern VdpDecoderRender* vdp_decoder_render;
extern VdpOutputSurfaceCreate* vdp_output_surface_create;
extern VdpOutputSurfaceDestroy* vdp_output_surface_destroy;
extern VdpOutputSurfaceGetBitsNative* vdp_output_surface_get_bits_native;
extern VdpOutputSurfaceGetParameters* vdp_output_surface_get_parameters;
extern VdpVideoMixerCreate* vdp_video_mixer_create;
extern VdpVideoMixerDestroy* vdp_video_mixer_destroy;
extern VdpVideoMixerSetAttributeValues* vdp_video_mixer_set_attribute_values;
extern VdpVideoMixerRender* vdp_video_mixer_render;
extern VdpPresentationQueueCreate* vdp_presentation_queue_create;
extern VdpPresentationQueueDestroy* vdp_presentation_queue_destroy;
extern VdpPresentationQueueGetTime* vdp_presentation_queue_get_time;
extern VdpPresentationQueueTargetCreateX11* vdp_presentation_queue_target_create_x11;
extern VdpPresentationQueueTargetDestroy* vdp_presentation_queue_target_destroy;
extern VdpPresentationQueueQuerySurfaceStatus* vdp_presentation_queue_query_surface_status;
extern VdpPresentationQueueDisplay* vdp_presentation_queue_display;
extern VdpPresentationQueueBlockUntilSurfaceIdle* vdp_presentation_queue_block_until_surface_idle;

void checkedVdpCall(VdpStatus status, const char* funcName);

/**
 * Call vdpau function with error checking and optional logging.
 */
#define VDP(CALL) do \
{ \
    if (ini().outputVdpauCalls) \
        fprintf(stderr, "VDPAU CALL: %s\n", #CALL); \
    checkedVdpCall(CALL, #CALL); \
} while(0)

void vdpCheckHandle(uint32_t handle, const char* name);

/**
 * Attempt to use Nx extension to vdpau_sunxi which allows to work without X11; if fails, then
 * initialize VDPAU with X11.
 * @param outDrawable Can be used for vdp_presentation_queue_target_create_x11().
 */
VdpDevice createVdpDevice(Drawable* outDrawable);

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

uint32_t calcYuvNativeQuickHash(const YuvNative* yuvNative);

void logYuvNative(const YuvNative* yuvNative);

const char* vdpChromaTypeToStr(VdpChromaType v);

const char* vdpYCbCrFormatToStr(VdpYCbCrFormat v);

void debugDumpYuvSurfaceToFiles(
    const char *filesPath, VdpVideoSurface surface,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize);
