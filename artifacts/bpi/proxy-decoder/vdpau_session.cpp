#include "vdpau_session.h"

#include "proxy_decoder_utils.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "proxydecoder[vdpau]: "

VdpauSession::VdpauSession(int frameWidth, int frameHeight):
    m_frameWidth(frameWidth),
    m_frameHeight(frameHeight)
{
    if (ini().videoSurfaceCount < 1 || ini().videoSurfaceCount > 16)
    {
        NX_PRINT << "WARNING: configuration param videoSurfaceCount is "
            << ini().videoSurfaceCount << " but should be 1..16; defaults to 1.";
        const_cast<int&>(ini().videoSurfaceCount) = 1;
    }
    if (ini().outputSurfaceCount < 0 || ini().outputSurfaceCount > 255)
    {
        NX_PRINT << "WARNING: configuration param outputSurfaceCount is "
            << ini().outputSurfaceCount << " but should be 1..100; defaults to 1.";
        const_cast<int&>(ini().outputSurfaceCount) = 1;
    }
    NX_PRINT << "Initializing VDPAU; using "
        << ini().videoSurfaceCount << " video surfaces, "
        << ini().outputSurfaceCount << " output surfaces";

    Drawable drawable;
    m_vdpDevice = createVdpDevice(&drawable);

    // The following initializations are needed for decodeToDisplay().

    VDP(vdp_video_mixer_create(m_vdpDevice,
        /*feature_count*/ 0,
        /*features*/ nullptr,
        /*parameter_count*/ 0,
        /*parameters*/ nullptr,
        /*parameter_values*/ nullptr,
        &m_vdpMixer));
    vdpCheckHandle(m_vdpMixer, "Mixer");

    if (!ini().disableCscMatrix)
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

    for (int i = 0; i < ini().outputSurfaceCount; ++i)
    {
        VdpOutputSurface outputSurface = VDP_INVALID_HANDLE;
        VDP(vdp_output_surface_create(m_vdpDevice,
            VDP_RGBA_FORMAT_B8G8R8A8, m_frameWidth, m_frameHeight, &outputSurface));
        vdpCheckHandle(outputSurface, "Output Surface");
        m_outputSurfaces.push_back(outputSurface);
    }

    // "VDPAU initialized OK" message to be printed by caller.
}

VdpauSession::~VdpauSession()
{
    // "Deinitializing VDPAU..." message to be printed by caller.

    for (auto& surface: m_outputSurfaces)
    {
        if (surface != VDP_INVALID_HANDLE)
            VDP(vdp_output_surface_destroy(surface));
        surface = VDP_INVALID_HANDLE;
    }

    if (m_vdpDevice != VDP_INVALID_HANDLE)
        VDP(vdp_device_destroy(m_vdpDevice));

    if (m_vdpQueue != VDP_INVALID_HANDLE)
        VDP(vdp_presentation_queue_destroy(m_vdpQueue));

    if (m_vdpTarget != VDP_INVALID_HANDLE)
        VDP(vdp_presentation_queue_target_destroy(m_vdpTarget));

    if (m_vdpMixer != VDP_INVALID_HANDLE)
        VDP(vdp_video_mixer_destroy(m_vdpMixer));

    NX_OUTPUT << "VDPAU deinitialized OK";
}

void VdpauSession::displayVideoSurface(
    VdpVideoSurface videoSurface, const ProxyDecoder::Rect* rect)
{
    VdpOutputSurface outputSurface = obtainOutputSurface();
    assert(outputSurface != VDP_INVALID_HANDLE);

    const VdpRect vdpRect = obtainDestinationVideoRect(rect);

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
        /*destination_video_rect*/ &vdpRect, //< Can be specified.
        /*layer_count*/ 0, //< Other values not implemented.
        /*layers*/ nullptr //< Not implemented.
    ));

    VDP(vdp_presentation_queue_display(m_vdpQueue, outputSurface,
        /*clip_width*/ m_frameWidth, /*clip_height*/ m_frameHeight,
        /*earliest_presentation_time, not implemented*/ 0));

    releaseOutputSurface(outputSurface);
}

VdpRect VdpauSession::obtainDestinationVideoRect(const ProxyDecoder::Rect* rect)
{
    int windowWidth = 0;
    int windowHeight = 0;
    if (!rect) //< fullscreen
    {
        if (g_fullScreenWidthHeight == 0) //< Fallback if VDPAU didn't provide screen size.
        {
            windowWidth = 1920;
            windowHeight = 1080;
            if (!m_videoRectReported)
            {
                m_videoRectReported = true;
                NX_PRINT << "WARNING: Full-screen requested but VDPAU did not provide screen size; "
                    << "using defaults: " << windowWidth << " x " << windowHeight;
            }
        }
        else
        {
            windowWidth = g_fullScreenWidthHeight & 0xFFFF;
            windowHeight = (g_fullScreenWidthHeight >> 16) & 0xFFFF;
            if (!m_videoRectReported)
            {
                m_videoRectReported = true;
                NX_OUTPUT << "Full-screen requested; screen size: "
                        << windowWidth << " x " << windowHeight;
            }
        }
    }
    else
    {
        windowWidth = rect->width;
        windowHeight = rect->height;
    }

    // Video position relative to window.
    int videoX = 0;
    int videoY = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    if (windowWidth * m_frameHeight <= windowHeight * m_frameWidth)
    {
        // Video is "wider" than screen.
        videoWidth = windowWidth;
        videoHeight = (m_frameHeight * windowWidth) / m_frameWidth;
        videoX = 0;
        videoY = (windowHeight - videoHeight) / 2;
    }
    else
    {
        // Screen is "wider" than video.
        videoWidth = (m_frameWidth * windowHeight) / m_frameHeight;
        videoHeight = windowHeight;
        videoX = (windowWidth - videoWidth) / 2;
        videoY = 0;
    }

    VdpRect result;
    result.x0 = (rect ? rect->x : 0) + videoX;
    result.x1 = result.x0 + videoWidth;
    result.y0 = (rect ? rect->y : 0) + videoY;
    result.y1 = result.y0 + videoHeight;
    return result;
}

VdpOutputSurface VdpauSession::obtainOutputSurface()
{
    VdpOutputSurface outputSurface = VDP_INVALID_HANDLE;
    if (ini().outputSurfaceCount == 0)
    {
        VDP(vdp_output_surface_create(m_vdpDevice,
            VDP_RGBA_FORMAT_B8G8R8A8, m_frameWidth, m_frameHeight, &outputSurface));
        vdpCheckHandle(outputSurface, "Output Surface");
    }
    else
    {
        const int outputSurfaceIndex = m_outputSurfaceIndex;
        m_outputSurfaceIndex = (m_outputSurfaceIndex + 1) % ini().outputSurfaceCount;
        outputSurface = m_outputSurfaces[outputSurfaceIndex];
        NX_OUTPUT << format("Using outputSurface %02d of %d {handle #%02d}",
                outputSurfaceIndex, ini().outputSurfaceCount, outputSurface);
    }
    return outputSurface;
}

void VdpauSession::releaseOutputSurface(VdpOutputSurface outputSurface) const
{
    if (ini().outputSurfaceCount == 0)
        VDP(vdp_output_surface_destroy(outputSurface));
}
