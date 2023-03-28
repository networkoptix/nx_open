// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "profile_tier_level.h"

namespace nx::media::hevc {

bool ProfileTierLevel::read(nx::utils::BitStreamReader& reader, int maxNumSubLayersMinus1)
{
    general.profile_present_flag = true;
    general.level_present_flag = true;
    general.read(reader);
    sub_layers.resize(maxNumSubLayersMinus1);
    for(size_t i = 0; i < sub_layers.size(); ++i) {
        sub_layers[i].profile_present_flag = reader.getBits(1);
        sub_layers[i].level_present_flag = reader.getBits(1);
    }
    if (!sub_layers.empty()) {
        for(size_t i = sub_layers.size(); i < 8; ++i) {
            reader.skipBits(2);
        }
    }
    for(size_t i = 0; i < sub_layers.size(); ++i) {
        sub_layers[i].read(reader);
    }
    return true;
}

bool ProfileTierLevel::Layer::read(nx::utils::BitStreamReader& reader)
{
    if (profile_present_flag)
    {
        profile_space = reader.getBits(2);
        tier_flag = reader.getBits(1);
        profile_idc = reader.getBits(5);
        for(uint32_t i = 0; i < 32; ++i)
            profile_compatibility_flags[i] = reader.getBits(1);

        progressive_source_flag = reader.getBits(1);
        interlaced_source_flag = reader.getBits(1);
        non_packed_constraint_flag = reader.getBits(1);
        frame_only_constraint_flag = reader.getBits(1);
        reader.skipBits(32);
        reader.skipBits(12);
    }
    if (level_present_flag) {
        level_idc = reader.getBits(8);
    }
    return true;
}

uint64_t ProfileTierLevel::Layer::constraint_indicator_flags() const
{
    uint64_t result = 0;
    if (progressive_source_flag) result |= (1ull << 47);
    if (interlaced_source_flag) result |= (1ull<< 46);
    if (non_packed_constraint_flag) result |= (1ull << 45);
    if (frame_only_constraint_flag) result |= (1ull << 44);
    return result;
}

} // namespace nx::media::hevc
