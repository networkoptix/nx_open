#include "vdpau_private.h"

static void log_call(const char* func_name)
{
    if (ini.enableOutput)
        VDPAU_DBG("%s() BEGIN", func_name);
}

static VdpStatus log_result(VdpStatus result, const char* func_name)
{
    if (ini.enableOutput)
    {
        if (result == VDP_STATUS_OK)
        {
            VDPAU_DBG("%s() END", func_name);
        }
        else
        {
            VDPAU_DBG("%s() END -> %s", func_name,
                vdp_get_error_string(result));
        }
    }
    return result;
}

static void log_in_uint(uint32_t value, const char* name)
{
    if (ini.enableOutput)
        VDPAU_DBG("    [in] %s: %u", name, value);
}

static void log_in_uint64(uint64_t value, const char* name)
{
    if (ini.enableOutput)
        VDPAU_DBG("    [in] %s: %llu", name, value);
}

static void log_in_int(int value, const char* name)
{
    if (ini.enableOutput)
        VDPAU_DBG("    [in] %s: %d", name, value);
}

static void log_in_Display(const Display* display, const char* name)
{
    if (ini.enableOutput)
    {
        if (display == NULL)
            VDPAU_DBG("    [in] %s: null", name);
        else
            VDPAU_DBG("    [in] %s: 0x%08X", name, (uint32_t) display);
    }
}

static void log_in_enum(const char* str, int value, const char* name)
{
    if (ini.enableOutput)
    {
        if (str == NULL)
            VDPAU_DBG("    [in] %s: unknown %d", name, value);
        else
            VDPAU_DBG("    [in] %s: %s", name, str);
    }
}

static void log_in_VdpColor(const VdpColor* color, const char* name)
{
    if (ini.enableOutput)
    {
        if (color == NULL)
        {
            VDPAU_DBG("    [in] %s: null", name);
        }
        else if (color->red == 0 && color->green == 0 && color->blue == 0 && color->alpha == 0)
        {
            VDPAU_DBG("    [in] %s: all-zero", name);
        }
        else
        {
            VDPAU_DBG("    [in] %s: {red: %f, green: %f, blue: %f, alpha: %f}", name,
                color->red, color->green, color->blue, color->alpha);
        }
    }
}

static void log_in_VdpCSCMatrix(const VdpCSCMatrix* p, const char* name)
{
    if (p == NULL)
    {
        VDPAU_DBG("    [in] %s: null", name);
    }
    else
    {
        VDPAU_DBG("    [in] %s:", name);
        VDPAU_DBG("    {");
        VDPAU_DBG("        {%f, %f, %f, %f},", (*p)[0][0], (*p)[0][1], (*p)[0][2], (*p)[0][3]);
        VDPAU_DBG("        {%f, %f, %f, %f},", (*p)[1][0], (*p)[1][1], (*p)[1][2], (*p)[1][3]);
        VDPAU_DBG("        {%f, %f, %f, %f},", (*p)[2][0], (*p)[2][1], (*p)[2][2], (*p)[2][3]);
        VDPAU_DBG("    }");
    }
}

static void log_in_VdpVideoMixerAttribute_value(
    VdpVideoMixerAttribute attribute, const void* value, const char* name)
{
    if (ini.enableOutput)
    {
        switch (attribute)
        {
            case VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX:
                log_in_VdpCSCMatrix((const VdpCSCMatrix*) value, name);
                break;
            default:
                if (value == NULL)
                    VDPAU_DBG("    [in] %s: null", name);
                else
                    VDPAU_DBG("    [in] %s: 0x%08X", name, (uint32_t) value);
        }
    }
}

static void log_out_uint(const uint32_t* pValue, const char* name)
{
    if (ini.enableOutput)
    {
        if (pValue != NULL)
            VDPAU_DBG("    [out] *%s: %u", name, *pValue);
        else
            VDPAU_DBG("    [out] %s is null", name);
    }
}

static void log_out_ptr(void ** pValue, const char* name)
{
    if (ini.enableOutput)
    {
        if (pValue == NULL)
            VDPAU_DBG("    [out] %s is null", name);
        else if (*pValue == NULL)
            VDPAU_DBG("    [out] *%s: null", name);
        else
            VDPAU_DBG("    [out] *%s: 0x%08X", name, (uint32_t) (*pValue));
    }
}

//-------------------------------------------------------------------------------------------------
// Stringifying vdpau.h constants

static const char* VdpRGBAFormat_str(VdpRGBAFormat value)
{
    switch (value)
    {
        case VDP_RGBA_FORMAT_B8G8R8A8: return "VDP_RGBA_FORMAT_B8G8R8A8";
        case VDP_RGBA_FORMAT_R8G8B8A8: return "VDP_RGBA_FORMAT_R8G8B8A8";
        case VDP_RGBA_FORMAT_R10G10B10A2: return "VDP_RGBA_FORMAT_R10G10B10A2";
        case VDP_RGBA_FORMAT_B10G10R10A2: return "VDP_RGBA_FORMAT_B10G10R10A2";
        case VDP_RGBA_FORMAT_A8: return "VDP_RGBA_FORMAT_A8";
        default: return NULL;
    }
}

static const char* VdpVideoMixerAttribute_str(VdpVideoMixerAttribute value)
{
    switch (value)
    {
        case VDP_VIDEO_MIXER_ATTRIBUTE_BACKGROUND_COLOR: return "VDP_VIDEO_MIXER_ATTRIBUTE_BACKGROUND_COLOR";
        case VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX: return "VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX";
        case VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL: return "VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL";
        case VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL: return "VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL";
        case VDP_VIDEO_MIXER_ATTRIBUTE_LUMA_KEY_MIN_LUMA: return "VDP_VIDEO_MIXER_ATTRIBUTE_LUMA_KEY_MIN_LUMA";
        case VDP_VIDEO_MIXER_ATTRIBUTE_LUMA_KEY_MAX_LUMA: return "VDP_VIDEO_MIXER_ATTRIBUTE_LUMA_KEY_MAX_LUMA";
        case VDP_VIDEO_MIXER_ATTRIBUTE_SKIP_CHROMA_DEINTERLACE: return "VDP_VIDEO_MIXER_ATTRIBUTE_SKIP_CHROMA_DEINTERLACE";
        default: return NULL;
    }
}

//-------------------------------------------------------------------------------------------------
// Logging wrappers written manually for calls which are not in functions[].

VdpStatus vdp_imp_device_create_x11(
    Display* display,
    int screen,
    VdpDevice* device,
    VdpGetProcAddress** get_proc_address)
{
    log_call("vdp_device_create_x11");
    log_in_Display(display, "display");
    log_in_int(screen, "screen");
    VdpStatus result = vdp_device_create_x11(
        display, screen, device, get_proc_address);
    log_out_uint(device, "device");
    log_out_ptr((void**) get_proc_address, "get_proc_address");
    return log_result(result, "vdp_device_create_x11");
}

VdpStatus log_presentation_queue_target_create_x11(
    VdpDevice device,
    Drawable drawable,
    VdpPresentationQueueTarget* target)
{
    log_call("vdp_presentation_queue_target_create_x11");
    log_in_uint(device, "device");
    log_in_uint(drawable, "drawable");
    VdpStatus result = vdp_presentation_queue_target_create_x11(
        device, drawable, target);
    log_out_uint(target, "target");
    return log_result(result, "vdp_presentation_queue_target_create_x11");
}

//-------------------------------------------------------------------------------------------------
// Logging wrappers generated by vdpautool.

VdpStatus log_get_api_version(
    uint32_t * api_version)
{
    log_call("vdp_get_api_version");
    VdpStatus result = vdp_get_api_version(
        api_version);
    return log_result(result, "vdp_get_api_version");
}

VdpStatus log_get_information_string(
    char const * * information_string)
{
    log_call("vdp_get_information_string");
    VdpStatus result = vdp_get_information_string(
        information_string);
    return log_result(result, "vdp_get_information_string");
}

VdpStatus log_device_destroy(
    VdpDevice device)
{
    log_call("vdp_device_destroy");
    VdpStatus result = vdp_device_destroy(
        device);
    return log_result(result, "vdp_device_destroy");
}

VdpStatus log_generate_csc_matrix(
    VdpProcamp * procamp,
    VdpColorStandard standard,
    VdpCSCMatrix * csc_matrix)
{
    log_call("vdp_generate_csc_matrix");
    VdpStatus result = vdp_generate_csc_matrix(
        procamp, standard, csc_matrix);
    return log_result(result, "vdp_generate_csc_matrix");
}

VdpStatus log_video_surface_query_capabilities(
    VdpDevice device,
    VdpChromaType surface_chroma_type,
    VdpBool * is_supported,
    uint32_t * max_width,
    uint32_t * max_height)
{
    log_call("vdp_video_surface_query_capabilities");
    VdpStatus result = vdp_video_surface_query_capabilities(
        device, surface_chroma_type, is_supported, max_width, max_height);
    return log_result(result, "vdp_video_surface_query_capabilities");
}

VdpStatus log_video_surface_query_get_put_bits_y_cb_cr_capabilities(
    VdpDevice device,
    VdpChromaType surface_chroma_type,
    VdpYCbCrFormat bits_ycbcr_format,
    VdpBool * is_supported)
{
    log_call("vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities");
    VdpStatus result = vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(
        device, surface_chroma_type, bits_ycbcr_format, is_supported);
    return log_result(result, "vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities");
}

VdpStatus log_video_surface_create(
    VdpDevice device,
    VdpChromaType chroma_type,
    uint32_t width,
    uint32_t height,
    VdpVideoSurface * surface)
{
    log_call("vdp_video_surface_create");
    VdpStatus result = vdp_video_surface_create(
        device, chroma_type, width, height, surface);
    return log_result(result, "vdp_video_surface_create");
}

VdpStatus log_video_surface_destroy(
    VdpVideoSurface surface)
{
    log_call("vdp_video_surface_destroy");
    VdpStatus result = vdp_video_surface_destroy(
        surface);
    return log_result(result, "vdp_video_surface_destroy");
}

VdpStatus log_video_surface_get_parameters(
    VdpVideoSurface surface,
    VdpChromaType * chroma_type,
    uint32_t * width,
    uint32_t * height)
{
    log_call("vdp_video_surface_get_parameters");
    VdpStatus result = vdp_video_surface_get_parameters(
        surface, chroma_type, width, height);
    return log_result(result, "vdp_video_surface_get_parameters");
}

VdpStatus log_video_surface_get_bits_y_cb_cr(
    VdpVideoSurface surface,
    VdpYCbCrFormat destination_ycbcr_format,
    void * const * destination_data,
    uint32_t const * destination_pitches)
{
    log_call("vdp_video_surface_get_bits_y_cb_cr");
    VdpStatus result = vdp_video_surface_get_bits_y_cb_cr(
        surface, destination_ycbcr_format, destination_data, destination_pitches);
    return log_result(result, "vdp_video_surface_get_bits_y_cb_cr");
}

VdpStatus log_video_surface_put_bits_y_cb_cr(
    VdpVideoSurface surface,
    VdpYCbCrFormat source_ycbcr_format,
    void const * const * source_data,
    uint32_t const * source_pitches)
{
    log_call("vdp_video_surface_put_bits_y_cb_cr");
    VdpStatus result = vdp_video_surface_put_bits_y_cb_cr(
        surface, source_ycbcr_format, source_data, source_pitches);
    return log_result(result, "vdp_video_surface_put_bits_y_cb_cr");
}

VdpStatus log_output_surface_query_capabilities(
    VdpDevice device,
    VdpRGBAFormat surface_rgba_format,
    VdpBool * is_supported,
    uint32_t * max_width,
    uint32_t * max_height)
{
    log_call("vdp_output_surface_query_capabilities");
    VdpStatus result = vdp_output_surface_query_capabilities(
        device, surface_rgba_format, is_supported, max_width, max_height);
    return log_result(result, "vdp_output_surface_query_capabilities");
}

VdpStatus log_output_surface_query_get_put_bits_native_capabilities(
    VdpDevice device,
    VdpRGBAFormat surface_rgba_format,
    VdpBool * is_supported)
{
    log_call("vdp_output_surface_query_get_put_bits_native_capabilities");
    VdpStatus result = vdp_output_surface_query_get_put_bits_native_capabilities(
        device, surface_rgba_format, is_supported);
    return log_result(result, "vdp_output_surface_query_get_put_bits_native_capabilities");
}

VdpStatus log_output_surface_query_put_bits_indexed_capabilities(
    VdpDevice device,
    VdpRGBAFormat surface_rgba_format,
    VdpIndexedFormat bits_indexed_format,
    VdpColorTableFormat color_table_format,
    VdpBool * is_supported)
{
    log_call("vdp_output_surface_query_put_bits_indexed_capabilities");
    VdpStatus result = vdp_output_surface_query_put_bits_indexed_capabilities(
        device, surface_rgba_format, bits_indexed_format, color_table_format, is_supported);
    return log_result(result, "vdp_output_surface_query_put_bits_indexed_capabilities");
}

VdpStatus log_output_surface_query_put_bits_y_cb_cr_capabilities(
    VdpDevice device,
    VdpRGBAFormat surface_rgba_format,
    VdpYCbCrFormat bits_ycbcr_format,
    VdpBool * is_supported)
{
    log_call("vdp_output_surface_query_put_bits_y_cb_cr_capabilities");
    VdpStatus result = vdp_output_surface_query_put_bits_y_cb_cr_capabilities(
        device, surface_rgba_format, bits_ycbcr_format, is_supported);
    return log_result(result, "vdp_output_surface_query_put_bits_y_cb_cr_capabilities");
}

VdpStatus log_output_surface_create(
    VdpDevice device,
    VdpRGBAFormat rgba_format,
    uint32_t width,
    uint32_t height,
    VdpOutputSurface * surface)
{
    log_call("vdp_output_surface_create");
    log_in_uint(device, "device");
    log_in_enum(VdpRGBAFormat_str(rgba_format), rgba_format, "rgba_format");
    log_in_uint(width, "width");
    log_in_uint(height, "height");
    VdpStatus result = vdp_output_surface_create(
        device, rgba_format, width, height, surface);
    log_out_uint(surface, "surface");
    return log_result(result, "vdp_output_surface_create");
}

VdpStatus log_output_surface_destroy(
    VdpOutputSurface surface)
{
    log_call("vdp_output_surface_destroy");
    VdpStatus result = vdp_output_surface_destroy(
        surface);
    return log_result(result, "vdp_output_surface_destroy");
}

VdpStatus log_output_surface_get_parameters(
    VdpOutputSurface surface,
    VdpRGBAFormat * rgba_format,
    uint32_t * width,
    uint32_t * height)
{
    log_call("vdp_output_surface_get_parameters");
    VdpStatus result = vdp_output_surface_get_parameters(
        surface, rgba_format, width, height);
    return log_result(result, "vdp_output_surface_get_parameters");
}

VdpStatus log_output_surface_get_bits_native(
    VdpOutputSurface surface,
    VdpRect const * source_rect,
    void * const * destination_data,
    uint32_t const * destination_pitches)
{
    log_call("vdp_output_surface_get_bits_native");
    VdpStatus result = vdp_output_surface_get_bits_native(
        surface, source_rect, destination_data, destination_pitches);
    return log_result(result, "vdp_output_surface_get_bits_native");
}

VdpStatus log_output_surface_put_bits_native(
    VdpOutputSurface surface,
    void const * const * source_data,
    uint32_t const * source_pitches,
    VdpRect const * destination_rect)
{
    log_call("vdp_output_surface_put_bits_native");
    VdpStatus result = vdp_output_surface_put_bits_native(
        surface, source_data, source_pitches, destination_rect);
    return log_result(result, "vdp_output_surface_put_bits_native");
}

VdpStatus log_output_surface_put_bits_indexed(
    VdpOutputSurface surface,
    VdpIndexedFormat source_indexed_format,
    void const * const * source_data,
    uint32_t const * source_pitch,
    VdpRect const * destination_rect,
    VdpColorTableFormat color_table_format,
    void const * color_table)
{
    log_call("vdp_output_surface_put_bits_indexed");
    VdpStatus result = vdp_output_surface_put_bits_indexed(
        surface, source_indexed_format, source_data, source_pitch, destination_rect, color_table_format, color_table);
    return log_result(result, "vdp_output_surface_put_bits_indexed");
}

VdpStatus log_output_surface_put_bits_y_cb_cr(
    VdpOutputSurface surface,
    VdpYCbCrFormat source_ycbcr_format,
    void const * const * source_data,
    uint32_t const * source_pitches,
    VdpRect const * destination_rect,
    VdpCSCMatrix const * csc_matrix)
{
    log_call("vdp_output_surface_put_bits_y_cb_cr");
    VdpStatus result = vdp_output_surface_put_bits_y_cb_cr(
        surface, source_ycbcr_format, source_data, source_pitches, destination_rect, csc_matrix);
    return log_result(result, "vdp_output_surface_put_bits_y_cb_cr");
}

VdpStatus log_bitmap_surface_query_capabilities(
    VdpDevice device,
    VdpRGBAFormat surface_rgba_format,
    VdpBool * is_supported,
    uint32_t * max_width,
    uint32_t * max_height)
{
    log_call("vdp_bitmap_surface_query_capabilities");
    VdpStatus result = vdp_bitmap_surface_query_capabilities(
        device, surface_rgba_format, is_supported, max_width, max_height);
    return log_result(result, "vdp_bitmap_surface_query_capabilities");
}

VdpStatus log_bitmap_surface_create(
    VdpDevice device,
    VdpRGBAFormat rgba_format,
    uint32_t width,
    uint32_t height,
    VdpBool frequently_accessed,
    VdpBitmapSurface * surface)
{
    log_call("vdp_bitmap_surface_create");
    VdpStatus result = vdp_bitmap_surface_create(
        device, rgba_format, width, height, frequently_accessed, surface);
    return log_result(result, "vdp_bitmap_surface_create");
}

VdpStatus log_bitmap_surface_destroy(
    VdpBitmapSurface surface)
{
    log_call("vdp_bitmap_surface_destroy");
    VdpStatus result = vdp_bitmap_surface_destroy(
        surface);
    return log_result(result, "vdp_bitmap_surface_destroy");
}

VdpStatus log_bitmap_surface_get_parameters(
    VdpBitmapSurface surface,
    VdpRGBAFormat * rgba_format,
    uint32_t * width,
    uint32_t * height,
    VdpBool * frequently_accessed)
{
    log_call("vdp_bitmap_surface_get_parameters");
    VdpStatus result = vdp_bitmap_surface_get_parameters(
        surface, rgba_format, width, height, frequently_accessed);
    return log_result(result, "vdp_bitmap_surface_get_parameters");
}

VdpStatus log_bitmap_surface_put_bits_native(
    VdpBitmapSurface surface,
    void const * const * source_data,
    uint32_t const * source_pitches,
    VdpRect const * destination_rect)
{
    log_call("vdp_bitmap_surface_put_bits_native");
    VdpStatus result = vdp_bitmap_surface_put_bits_native(
        surface, source_data, source_pitches, destination_rect);
    return log_result(result, "vdp_bitmap_surface_put_bits_native");
}

VdpStatus log_output_surface_render_output_surface(
    VdpOutputSurface destination_surface,
    VdpRect const * destination_rect,
    VdpOutputSurface source_surface,
    VdpRect const * source_rect,
    VdpColor const * colors,
    VdpOutputSurfaceRenderBlendState const * blend_state,
    uint32_t flags)
{
    log_call("vdp_output_surface_render_output_surface");
    VdpStatus result = vdp_output_surface_render_output_surface(
        destination_surface, destination_rect, source_surface, source_rect, colors, blend_state, flags);
    return log_result(result, "vdp_output_surface_render_output_surface");
}

VdpStatus log_output_surface_render_bitmap_surface(
    VdpOutputSurface destination_surface,
    VdpRect const * destination_rect,
    VdpBitmapSurface source_surface,
    VdpRect const * source_rect,
    VdpColor const * colors,
    VdpOutputSurfaceRenderBlendState const * blend_state,
    uint32_t flags)
{
    log_call("vdp_output_surface_render_bitmap_surface");
    VdpStatus result = vdp_output_surface_render_bitmap_surface(
        destination_surface, destination_rect, source_surface, source_rect, colors, blend_state, flags);
    return log_result(result, "vdp_output_surface_render_bitmap_surface");
}

VdpStatus log_decoder_query_capabilities(
    VdpDevice device,
    VdpDecoderProfile profile,
    VdpBool * is_supported,
    uint32_t * max_level,
    uint32_t * max_macroblocks,
    uint32_t * max_width,
    uint32_t * max_height)
{
    log_call("vdp_decoder_query_capabilities");
    VdpStatus result = vdp_decoder_query_capabilities(
        device, profile, is_supported, max_level, max_macroblocks, max_width, max_height);
    return log_result(result, "vdp_decoder_query_capabilities");
}

VdpStatus log_decoder_create(
    VdpDevice device,
    VdpDecoderProfile profile,
    uint32_t width,
    uint32_t height,
    uint32_t max_references,
    VdpDecoder * decoder)
{
    log_call("vdp_decoder_create");
    VdpStatus result = vdp_decoder_create(
        device, profile, width, height, max_references, decoder);
    return log_result(result, "vdp_decoder_create");
}

VdpStatus log_decoder_destroy(
    VdpDecoder decoder)
{
    log_call("vdp_decoder_destroy");
    VdpStatus result = vdp_decoder_destroy(
        decoder);
    return log_result(result, "vdp_decoder_destroy");
}

VdpStatus log_decoder_get_parameters(
    VdpDecoder decoder,
    VdpDecoderProfile * profile,
    uint32_t * width,
    uint32_t * height)
{
    log_call("vdp_decoder_get_parameters");
    VdpStatus result = vdp_decoder_get_parameters(
        decoder, profile, width, height);
    return log_result(result, "vdp_decoder_get_parameters");
}

VdpStatus log_decoder_render(
    VdpDecoder decoder,
    VdpVideoSurface target,
    VdpPictureInfo const * picture_info,
    uint32_t bitstream_buffer_count,
    VdpBitstreamBuffer const * bitstream_buffers)
{
    log_call("vdp_decoder_render");
    VdpStatus result = vdp_decoder_render(
        decoder, target, picture_info, bitstream_buffer_count, bitstream_buffers);
    return log_result(result, "vdp_decoder_render");
}

VdpStatus log_video_mixer_query_feature_support(
    VdpDevice device,
    VdpVideoMixerFeature feature,
    VdpBool * is_supported)
{
    log_call("vdp_video_mixer_query_feature_support");
    VdpStatus result = vdp_video_mixer_query_feature_support(
        device, feature, is_supported);
    return log_result(result, "vdp_video_mixer_query_feature_support");
}

VdpStatus log_video_mixer_query_parameter_support(
    VdpDevice device,
    VdpVideoMixerParameter parameter,
    VdpBool * is_supported)
{
    log_call("vdp_video_mixer_query_parameter_support");
    VdpStatus result = vdp_video_mixer_query_parameter_support(
        device, parameter, is_supported);
    return log_result(result, "vdp_video_mixer_query_parameter_support");
}

VdpStatus log_video_mixer_query_attribute_support(
    VdpDevice device,
    VdpVideoMixerAttribute attribute,
    VdpBool * is_supported)
{
    log_call("vdp_video_mixer_query_attribute_support");
    VdpStatus result = vdp_video_mixer_query_attribute_support(
        device, attribute, is_supported);
    return log_result(result, "vdp_video_mixer_query_attribute_support");
}

VdpStatus log_video_mixer_query_parameter_value_range(
    VdpDevice device,
    VdpVideoMixerParameter parameter,
    void * min_value,
    void * max_value)
{
    log_call("vdp_video_mixer_query_parameter_value_range");
    VdpStatus result = vdp_video_mixer_query_parameter_value_range(
        device, parameter, min_value, max_value);
    return log_result(result, "vdp_video_mixer_query_parameter_value_range");
}

VdpStatus log_video_mixer_query_attribute_value_range(
    VdpDevice device,
    VdpVideoMixerAttribute attribute,
    void * min_value,
    void * max_value)
{
    log_call("vdp_video_mixer_query_attribute_value_range");
    VdpStatus result = vdp_video_mixer_query_attribute_value_range(
        device, attribute, min_value, max_value);
    return log_result(result, "vdp_video_mixer_query_attribute_value_range");
}

VdpStatus log_video_mixer_create(
    VdpDevice device,
    uint32_t feature_count,
    VdpVideoMixerFeature const * features,
    uint32_t parameter_count,
    VdpVideoMixerParameter const * parameters,
    void const * const * parameter_values,
    VdpVideoMixer * mixer)
{
    log_call("vdp_video_mixer_create");
    VdpStatus result = vdp_video_mixer_create(
        device, feature_count, features, parameter_count, parameters, parameter_values, mixer);
    return log_result(result, "vdp_video_mixer_create");
}

VdpStatus log_video_mixer_set_feature_enables(
    VdpVideoMixer mixer,
    uint32_t feature_count,
    VdpVideoMixerFeature const * features,
    VdpBool const * feature_enables)
{
    log_call("vdp_video_mixer_set_feature_enables");
    VdpStatus result = vdp_video_mixer_set_feature_enables(
        mixer, feature_count, features, feature_enables);
    return log_result(result, "vdp_video_mixer_set_feature_enables");
}

VdpStatus log_video_mixer_set_attribute_values(
    VdpVideoMixer mixer,
    uint32_t attribute_count,
    VdpVideoMixerAttribute const * attributes,
    void const * const * attribute_values)
{
    log_call("vdp_video_mixer_set_attribute_values");
    log_in_uint(mixer, "mixer");
    log_in_uint(attribute_count, "attribute_count");
    if (attributes && attribute_values)
    {
        for (int i = 0; i < attribute_count; ++i)
        {
            char s[100];
            snprintf(s, sizeof(s), "attributes[%d]", i);
            log_in_enum(VdpVideoMixerAttribute_str(attributes[i]), attributes[i], s);
            snprintf(s, sizeof(s), "attribute_values[%d]", i);
            log_in_VdpVideoMixerAttribute_value(attributes[i], attribute_values[i], s);
        }
    }
    VdpStatus result = vdp_video_mixer_set_attribute_values(
        mixer, attribute_count, attributes, attribute_values);
    return log_result(result, "vdp_video_mixer_set_attribute_values");
}

VdpStatus log_video_mixer_get_feature_support(
    VdpVideoMixer mixer,
    uint32_t feature_count,
    VdpVideoMixerFeature const * features,
    VdpBool * feature_supports)
{
    log_call("vdp_video_mixer_get_feature_support");
    VdpStatus result = vdp_video_mixer_get_feature_support(
        mixer, feature_count, features, feature_supports);
    return log_result(result, "vdp_video_mixer_get_feature_support");
}

VdpStatus log_video_mixer_get_feature_enables(
    VdpVideoMixer mixer,
    uint32_t feature_count,
    VdpVideoMixerFeature const * features,
    VdpBool * feature_enables)
{
    log_call("vdp_video_mixer_get_feature_enables");
    VdpStatus result = vdp_video_mixer_get_feature_enables(
        mixer, feature_count, features, feature_enables);
    return log_result(result, "vdp_video_mixer_get_feature_enables");
}

VdpStatus log_video_mixer_get_parameter_values(
    VdpVideoMixer mixer,
    uint32_t parameter_count,
    VdpVideoMixerParameter const * parameters,
    void * const * parameter_values)
{
    log_call("vdp_video_mixer_get_parameter_values");
    VdpStatus result = vdp_video_mixer_get_parameter_values(
        mixer, parameter_count, parameters, parameter_values);
    return log_result(result, "vdp_video_mixer_get_parameter_values");
}

VdpStatus log_video_mixer_get_attribute_values(
    VdpVideoMixer mixer,
    uint32_t attribute_count,
    VdpVideoMixerAttribute const * attributes,
    void * const * attribute_values)
{
    log_call("vdp_video_mixer_get_attribute_values");
    VdpStatus result = vdp_video_mixer_get_attribute_values(
        mixer, attribute_count, attributes, attribute_values);
    return log_result(result, "vdp_video_mixer_get_attribute_values");
}

VdpStatus log_video_mixer_destroy(
    VdpVideoMixer mixer)
{
    log_call("vdp_video_mixer_destroy");
    VdpStatus result = vdp_video_mixer_destroy(
        mixer);
    return log_result(result, "vdp_video_mixer_destroy");
}

VdpStatus log_video_mixer_render(
    VdpVideoMixer mixer,
    VdpOutputSurface background_surface,
    VdpRect const * background_source_rect,
    VdpVideoMixerPictureStructure current_picture_structure,
    uint32_t video_surface_past_count,
    VdpVideoSurface const * video_surface_past,
    VdpVideoSurface video_surface_current,
    uint32_t video_surface_future_count,
    VdpVideoSurface const * video_surface_future,
    VdpRect const * video_source_rect,
    VdpOutputSurface destination_surface,
    VdpRect const * destination_rect,
    VdpRect const * destination_video_rect,
    uint32_t layer_count,
    VdpLayer const * layers)
{
    log_call("vdp_video_mixer_render");
    VdpStatus result = vdp_video_mixer_render(
        mixer, background_surface, background_source_rect, current_picture_structure, video_surface_past_count, video_surface_past, video_surface_current, video_surface_future_count, video_surface_future, video_source_rect, destination_surface, destination_rect, destination_video_rect, layer_count, layers);
    return log_result(result, "vdp_video_mixer_render");
}

VdpStatus log_presentation_queue_target_destroy(
    VdpPresentationQueueTarget presentation_queue_target)
{
    log_call("vdp_presentation_queue_target_destroy");
    VdpStatus result = vdp_presentation_queue_target_destroy(
        presentation_queue_target);
    return log_result(result, "vdp_presentation_queue_target_destroy");
}

VdpStatus log_presentation_queue_create(
    VdpDevice device,
    VdpPresentationQueueTarget presentation_queue_target,
    VdpPresentationQueue * presentation_queue)
{
    log_call("vdp_presentation_queue_create");
    VdpStatus result = vdp_presentation_queue_create(
        device, presentation_queue_target, presentation_queue);
    return log_result(result, "vdp_presentation_queue_create");
}

VdpStatus log_presentation_queue_destroy(
    VdpPresentationQueue presentation_queue)
{
    log_call("vdp_presentation_queue_destroy");
    VdpStatus result = vdp_presentation_queue_destroy(
        presentation_queue);
    return log_result(result, "vdp_presentation_queue_destroy");
}

VdpStatus log_presentation_queue_set_background_color(
    VdpPresentationQueue presentation_queue,
    VdpColor * const background_color)
{
    log_call("vdp_presentation_queue_set_background_color");
    log_in_uint(presentation_queue, "presentation_queue");
    log_in_VdpColor(background_color, "background_color");
    VdpStatus result = vdp_presentation_queue_set_background_color(
        presentation_queue, background_color);
    return log_result(result, "vdp_presentation_queue_set_background_color");
}

VdpStatus log_presentation_queue_get_background_color(
    VdpPresentationQueue presentation_queue,
    VdpColor * background_color)
{
    log_call("vdp_presentation_queue_get_background_color");
    VdpStatus result = vdp_presentation_queue_get_background_color(
        presentation_queue, background_color);
    return log_result(result, "vdp_presentation_queue_get_background_color");
}

VdpStatus log_presentation_queue_get_time(
    VdpPresentationQueue presentation_queue,
    VdpTime * current_time)
{
    log_call("vdp_presentation_queue_get_time");
    VdpStatus result = vdp_presentation_queue_get_time(
        presentation_queue, current_time);
    return log_result(result, "vdp_presentation_queue_get_time");
}

VdpStatus log_presentation_queue_display(
    VdpPresentationQueue presentation_queue,
    VdpOutputSurface surface,
    uint32_t clip_width,
    uint32_t clip_height,
    VdpTime earliest_presentation_time)
{
    log_call("vdp_presentation_queue_display");
    log_in_uint(presentation_queue, "presentation_queue");
    log_in_uint(surface, "surface");
    log_in_uint(clip_width, "clip_width");
    log_in_uint(clip_height, "clip_height");
    log_in_uint64(earliest_presentation_time, "earliest_presentation_time");
    VdpStatus result = vdp_presentation_queue_display(
        presentation_queue, surface, clip_width, clip_height, earliest_presentation_time);
    return log_result(result, "vdp_presentation_queue_display");
}

VdpStatus log_presentation_queue_block_until_surface_idle(
    VdpPresentationQueue presentation_queue,
    VdpOutputSurface surface,
    VdpTime * first_presentation_time)
{
    log_call("vdp_presentation_queue_block_until_surface_idle");
    VdpStatus result = vdp_presentation_queue_block_until_surface_idle(
        presentation_queue, surface, first_presentation_time);
    return log_result(result, "vdp_presentation_queue_block_until_surface_idle");
}

VdpStatus log_presentation_queue_query_surface_status(
    VdpPresentationQueue presentation_queue,
    VdpOutputSurface surface,
    VdpPresentationQueueStatus * status,
    VdpTime * first_presentation_time)
{
    log_call("vdp_presentation_queue_query_surface_status");
    VdpStatus result = vdp_presentation_queue_query_surface_status(
        presentation_queue, surface, status, first_presentation_time);
    return log_result(result, "vdp_presentation_queue_query_surface_status");
}

VdpStatus log_preemption_callback_register(
    VdpDevice device,
    VdpPreemptionCallback callback,
    void * context)
{
    log_call("vdp_preemption_callback_register");
    VdpStatus result = vdp_preemption_callback_register(
        device, callback, context);
    return log_result(result, "vdp_preemption_callback_register");
}

//-------------------------------------------------------------------------------------------------

void *const functions[] =
{
    [VDP_FUNC_ID_GET_ERROR_STRING]                                      = vdp_get_error_string,
    [VDP_FUNC_ID_GET_PROC_ADDRESS]                                      = vdp_get_proc_address,
    [VDP_FUNC_ID_GET_API_VERSION]                                       = log_get_api_version,
    [VDP_FUNC_ID_GET_INFORMATION_STRING]                                = log_get_information_string,
    [VDP_FUNC_ID_DEVICE_DESTROY]                                        = log_device_destroy,
    [VDP_FUNC_ID_GENERATE_CSC_MATRIX]                                   = log_generate_csc_matrix,
    [VDP_FUNC_ID_VIDEO_SURFACE_QUERY_CAPABILITIES]                      = log_video_surface_query_capabilities,
    [VDP_FUNC_ID_VIDEO_SURFACE_QUERY_GET_PUT_BITS_Y_CB_CR_CAPABILITIES] = log_video_surface_query_get_put_bits_y_cb_cr_capabilities,
    [VDP_FUNC_ID_VIDEO_SURFACE_CREATE]                                  = log_video_surface_create,
    [VDP_FUNC_ID_VIDEO_SURFACE_DESTROY]                                 = log_video_surface_destroy,
    [VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS]                          = log_video_surface_get_parameters,
    [VDP_FUNC_ID_VIDEO_SURFACE_GET_BITS_Y_CB_CR]                        = log_video_surface_get_bits_y_cb_cr,
    [VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR]                        = log_video_surface_put_bits_y_cb_cr,
    [VDP_FUNC_ID_OUTPUT_SURFACE_QUERY_CAPABILITIES]                     = log_output_surface_query_capabilities,
    [VDP_FUNC_ID_OUTPUT_SURFACE_QUERY_GET_PUT_BITS_NATIVE_CAPABILITIES] = log_output_surface_query_get_put_bits_native_capabilities,
    [VDP_FUNC_ID_OUTPUT_SURFACE_QUERY_PUT_BITS_INDEXED_CAPABILITIES]    = log_output_surface_query_put_bits_indexed_capabilities,
    [VDP_FUNC_ID_OUTPUT_SURFACE_QUERY_PUT_BITS_Y_CB_CR_CAPABILITIES]    = log_output_surface_query_put_bits_y_cb_cr_capabilities,
    [VDP_FUNC_ID_OUTPUT_SURFACE_CREATE]                                 = log_output_surface_create,
    [VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY]                                = log_output_surface_destroy,
    [VDP_FUNC_ID_OUTPUT_SURFACE_GET_PARAMETERS]                         = log_output_surface_get_parameters,
    [VDP_FUNC_ID_OUTPUT_SURFACE_GET_BITS_NATIVE]                        = log_output_surface_get_bits_native,
    [VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE]                        = log_output_surface_put_bits_native,
    [VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_INDEXED]                       = log_output_surface_put_bits_indexed,
    [VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_Y_CB_CR]                       = log_output_surface_put_bits_y_cb_cr,
    [VDP_FUNC_ID_BITMAP_SURFACE_QUERY_CAPABILITIES]                     = log_bitmap_surface_query_capabilities,
    [VDP_FUNC_ID_BITMAP_SURFACE_CREATE]                                 = log_bitmap_surface_create,
    [VDP_FUNC_ID_BITMAP_SURFACE_DESTROY]                                = log_bitmap_surface_destroy,
    [VDP_FUNC_ID_BITMAP_SURFACE_GET_PARAMETERS]                         = log_bitmap_surface_get_parameters,
    [VDP_FUNC_ID_BITMAP_SURFACE_PUT_BITS_NATIVE]                        = log_bitmap_surface_put_bits_native,
    [VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_OUTPUT_SURFACE]                  = log_output_surface_render_output_surface,
    [VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_BITMAP_SURFACE]                  = log_output_surface_render_bitmap_surface,
    [VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_VIDEO_SURFACE_LUMA]              = NULL,
    [VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES]                            = log_decoder_query_capabilities,
    [VDP_FUNC_ID_DECODER_CREATE]                                        = log_decoder_create,
    [VDP_FUNC_ID_DECODER_DESTROY]                                       = log_decoder_destroy,
    [VDP_FUNC_ID_DECODER_GET_PARAMETERS]                                = log_decoder_get_parameters,
    [VDP_FUNC_ID_DECODER_RENDER]                                        = log_decoder_render,
    [VDP_FUNC_ID_VIDEO_MIXER_QUERY_FEATURE_SUPPORT]                     = log_video_mixer_query_feature_support,
    [VDP_FUNC_ID_VIDEO_MIXER_QUERY_PARAMETER_SUPPORT]                   = log_video_mixer_query_parameter_support,
    [VDP_FUNC_ID_VIDEO_MIXER_QUERY_ATTRIBUTE_SUPPORT]                   = log_video_mixer_query_attribute_support,
    [VDP_FUNC_ID_VIDEO_MIXER_QUERY_PARAMETER_VALUE_RANGE]               = log_video_mixer_query_parameter_value_range,
    [VDP_FUNC_ID_VIDEO_MIXER_QUERY_ATTRIBUTE_VALUE_RANGE]               = log_video_mixer_query_attribute_value_range,
    [VDP_FUNC_ID_VIDEO_MIXER_CREATE]                                    = log_video_mixer_create,
    [VDP_FUNC_ID_VIDEO_MIXER_SET_FEATURE_ENABLES]                       = log_video_mixer_set_feature_enables,
    [VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES]                      = log_video_mixer_set_attribute_values,
    [VDP_FUNC_ID_VIDEO_MIXER_GET_FEATURE_SUPPORT]                       = log_video_mixer_get_feature_support,
    [VDP_FUNC_ID_VIDEO_MIXER_GET_FEATURE_ENABLES]                       = log_video_mixer_get_feature_enables,
    [VDP_FUNC_ID_VIDEO_MIXER_GET_PARAMETER_VALUES]                      = log_video_mixer_get_parameter_values,
    [VDP_FUNC_ID_VIDEO_MIXER_GET_ATTRIBUTE_VALUES]                      = log_video_mixer_get_attribute_values,
    [VDP_FUNC_ID_VIDEO_MIXER_DESTROY]                                   = log_video_mixer_destroy,
    [VDP_FUNC_ID_VIDEO_MIXER_RENDER]                                    = log_video_mixer_render,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY]                     = log_presentation_queue_target_destroy,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE]                             = log_presentation_queue_create,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY]                            = log_presentation_queue_destroy,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_SET_BACKGROUND_COLOR]               = log_presentation_queue_set_background_color,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_GET_BACKGROUND_COLOR]               = log_presentation_queue_get_background_color,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME]                           = log_presentation_queue_get_time,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY]                            = log_presentation_queue_display,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE]           = log_presentation_queue_block_until_surface_idle,
    [VDP_FUNC_ID_PRESENTATION_QUEUE_QUERY_SURFACE_STATUS]               = log_presentation_queue_query_surface_status,
    [VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER]                          = log_preemption_callback_register,
};

const int function_count = ARRAY_SIZE(functions);
