#include "vdpau_session.h"

#define OUTPUT_PREFIX "proxydecoder[vdpau]: "
#include "proxy_decoder_utils.h"

VdpauSession::VdpauSession(int frameWidth, int frameHeight)
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

    // The following initializations are needed for decodeToDisplay().

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

    for (int i = 0; i < conf.outputSurfaceCount; ++i)
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

    OUTPUT << "VDPAU deinitialized OK";
}

void VdpauSession::displayVideoSurface(
    VdpVideoSurface videoSurface, int x, int y, int width, int height)
{
    VdpOutputSurface outputSurface = obtainOutputSurface();
    assert(outputSurface != VDP_INVALID_HANDLE);

    VdpRect rect = obtainDestinationVideoRect(x, y, width, height);

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
        /*destination_video_rect*/ &rect, //< Can be specified.
        /*layer_count*/ 0, //< Other values not implemented.
        /*layers*/ nullptr //< Not implemented.
    ));

    VDP(vdp_presentation_queue_display(m_vdpQueue, outputSurface,
        /*clip_width*/ m_frameWidth, /*clip_height*/ m_frameHeight,
        /*earliest_presentation_time, not implemented*/ 0));

    releaseOutputSurface(outputSurface);
}

VdpRect VdpauSession::obtainDestinationVideoRect(int x, int y, int width, int height)
{
    int windowWidth = 0;
    int windowHeight = 0;
    if (x == 0 && y == 0 && width == 0 && height == 0) //< All zeroes mean full-screen.
    {
        if (fullScreenWidthHeight == 0) //< Fallback if VDPAU didn't provide screen size.
        {
            windowWidth = 1920;
            windowHeight = 1080;
            if (!m_videoRectReported)
            {
                m_videoRectReported = true;
                PRINT << "WARNING: Full-screen requested but VDPAU did not provide screen size; "
                    << "using defaults: " << windowWidth << " x " << windowHeight;
            }
        }
        else
        {
            windowWidth = fullScreenWidthHeight & 0xFFFF;
            windowHeight = (fullScreenWidthHeight >> 16) & 0xFFFF;
            if (!m_videoRectReported)
            {
                m_videoRectReported = true;
                OUTPUT << "Full-screen requested; screen size: "
                        << windowWidth << " x " << windowHeight;
            }
        }
    }
    else
    {
        windowWidth = width;
        windowHeight = height;
    }

    // Video position relative to window.
    int videoX = 0;
    int videoY = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    if (windowWidth * m_frameHeight <= windowHeight * m_frameWidth) //< Video is "wider" than screen.
    {
        videoWidth = windowWidth;
        videoHeight = (m_frameHeight * windowWidth) / m_frameWidth;
        videoX = 0;
        videoY = (windowHeight - videoHeight) / 2;
    }
    else //< Screen is "wider" than video.
    {
        videoWidth = (m_frameWidth * windowHeight) / m_frameHeight;
        videoHeight = windowHeight;
        videoX = (windowWidth - videoWidth) / 2;
        videoY = 0;
    }

    VdpRect rect;
    rect.x0 = x + videoX;
    rect.x1 = rect.x0 + videoWidth;
    rect.y0 = y + videoY;
    rect.y1 = rect.y0 + videoHeight;
    return rect;
}

VdpOutputSurface VdpauSession::obtainOutputSurface()
{
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
    return outputSurface;
}

void VdpauSession::releaseOutputSurface(VdpOutputSurface outputSurface) const
{
    if (conf.outputSurfaceCount == 0)
        VDP(vdp_output_surface_destroy(outputSurface));
}
