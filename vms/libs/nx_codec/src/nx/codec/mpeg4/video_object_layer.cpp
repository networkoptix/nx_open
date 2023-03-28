// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_object_layer.h"

#include <cmath>

#include <nx/utils/bit_stream.h>
#include <nx/utils/log/log.h>

namespace {

static constexpr int kAspectRatioExtended = 15;

struct Ratio { int w; int h; };
static constexpr Ratio kh263PixelAspect[16] = {
    {  0,  1 },
    {  1,  1 },
    { 12, 11 },
    { 10, 11 },
    { 16, 11 },
    { 40, 33 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
    {  0,  1 },
};

enum class Shape
{
    Rect = 0,
    Binary = 1,
    BinaryOnly = 2,
    Gray = 3,
};

inline void checkMarkerBit(nx::utils::BitStreamReader& reader, const std::string& description)
{
    if (!reader.getBit())
        throw nx::utils::BitStreamException("Invalid marker bit: " + description);
}

} // namespace

namespace nx::media::mpeg4 {

bool VideoObjectLayer::read(const uint8_t* data, int size)
{
    try
    {
        nx::utils::BitStreamReader reader(data, size);
        random_accessible_vol = reader.getBits(1);
        video_object_type_indication = reader.getBits(8);
        is_object_layer_identifier = reader.getBits(1);
        if (is_object_layer_identifier)
        {
            video_object_layer_verid = reader.getBits(4);
            video_object_layer_priority = reader.getBits(3);
        }
        else
        {
            video_object_layer_verid = 1; //< See ffmpeg.
        }

        aspect_ratio_info = reader.getBits(4);
        if (aspect_ratio_info == kAspectRatioExtended)
        {
            par_width = reader.getBits(8);
            par_height = reader.getBits(8);
        }
        else
        {
            par_width = kh263PixelAspect[aspect_ratio_info].w;
            par_height = kh263PixelAspect[aspect_ratio_info].h;
        }

        vol_control_parameters = reader.getBits(1);
        if (vol_control_parameters)
        {
            chroma_format = reader.getBits(2);
            low_delay = reader.getBits(1);
            vbv_parameters = reader.getBits(1);
            if (vbv_parameters)
            {
                first_half_bit_rate = reader.getBits(15);
                checkMarkerBit(reader, "first_half_bit_rate");
                latter_half_bit_rate = reader.getBits(15);
                checkMarkerBit(reader, "latter_half_bit_rate");
                first_half_vbv_buffer_size = reader.getBits(15);
                checkMarkerBit(reader, "first_half_vbv_buffer_size");
                latter_half_vbv_buffer_size = reader.getBits(3);
                first_half_vbv_occupancy = reader.getBits(11);
                checkMarkerBit(reader, "first_half_vbv_occupancy");
                latter_half_vbv_occupancy = reader.getBits(15);
                checkMarkerBit(reader, "latter_half_vbv_occupancy");
            }
        }
        video_object_layer_shape = reader.getBits(2);
        if (video_object_layer_shape != (int)Shape::Rect)
        {
            NX_DEBUG(this, "Shape not supported: %1", video_object_layer_shape);
            return false;
        }
        checkMarkerBit(reader, "video_object_layer_shape");
        vop_time_increment_resolution = reader.getBits(16);
        checkMarkerBit(reader, "vop_time_increment_resolution");
        fixed_vop_rate = reader.getBits(1);
        int timeIncrementBits = std::log2(vop_time_increment_resolution - 1) + 1;
        if (timeIncrementBits < 1)
            timeIncrementBits = 1;
        if (fixed_vop_rate)
            fixed_vop_time_increment = reader.getBits(timeIncrementBits);
        else
            fixed_vop_time_increment = 1;

        checkMarkerBit(reader, "fixed_vop_rate");
        video_object_layer_width = reader.getBits(13);
        checkMarkerBit(reader, "video_object_layer_width");
        video_object_layer_height = reader.getBits(13);
        checkMarkerBit(reader, "video_object_layer_height");
    }
    catch (const nx::utils::BitStreamException& ex)
    {
        NX_DEBUG(this, "Failed to parse Video Object Layer structure: %1", ex.what());
        return false;
    }
    return true;
}

} // namespace nx::media::mpeg4
