// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utils/media/bitStream.h>

namespace nx::media::hevc {

struct NX_VMS_COMMON_API ProfileTierLevel
{
    bool read(BitStreamReader& reader, int maxNumSubLayersMinus1);

    struct Layer
    {
        bool read(BitStreamReader& reader);
        uint64_t constraint_indicator_flags() const;

        bool profile_present_flag;
        bool level_present_flag;

        uint8_t profile_space;
        bool tier_flag;
        uint8_t profile_idc;
        std::bitset<32> profile_compatibility_flags;
        bool progressive_source_flag;
        bool interlaced_source_flag;
        bool non_packed_constraint_flag;
        bool frame_only_constraint_flag;
        uint8_t level_idc;
    };

    Layer general;
    std::vector<Layer> sub_layers;
};

} // namespace nx::media::hevc