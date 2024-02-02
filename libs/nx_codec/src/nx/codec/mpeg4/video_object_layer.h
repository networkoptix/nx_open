// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

namespace nx::media::mpeg4 {

struct NX_CODEC_API VideoObjectLayer
{
    bool read(const uint8_t* data, int size);

    bool random_accessible_vol = false;
    uint8_t video_object_type_indication = 0;
    bool is_object_layer_identifier = false;
    uint8_t video_object_layer_verid = 0;
    uint8_t video_object_layer_priority = 0;
    uint8_t aspect_ratio_info = 0;
    uint8_t par_width = 0;
    uint8_t par_height = 0;

    bool vol_control_parameters = false;
    // vol_control_parameters
    uint8_t chroma_format = 0;
    bool low_delay = false;
    bool vbv_parameters = false;
    // vbv_parameters
    uint8_t first_half_bit_rate = 0;
    uint8_t latter_half_bit_rate = 0;
    uint8_t first_half_vbv_buffer_size = 0;
    uint8_t latter_half_vbv_buffer_size = 0;
    uint8_t first_half_vbv_occupancy = 0;
    uint8_t latter_half_vbv_occupancy = 0;

    uint8_t video_object_layer_shape = 0;
    uint8_t video_object_layer_shape_extension = 0;
    uint16_t vop_time_increment_resolution = 0;
    bool fixed_vop_rate = false;
    uint16_t fixed_vop_time_increment = 0;

    uint16_t video_object_layer_width = 0;
    uint16_t video_object_layer_height = 0;
};

} // namespace nx::media::mpeg4
