// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <stdint.h>

namespace nx::media::hevc {

struct NX_VMS_COMMON_API HEVCDecoderConfigurationRecord
{
    bool read(const uint8_t* data, int size);
    bool write(uint8_t* data, int size) const;
    int size() const;

    uint8_t  configurationVersion;
    uint8_t  general_profile_space;
    bool general_tier_flag;
    uint8_t  general_profile_idc;
    uint32_t general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    uint8_t  general_level_idc;
    uint16_t min_spatial_segmentation_idc;
    uint8_t  parallelismType;
    uint8_t  chromaFormat;
    uint8_t  bitDepthLumaMinus8;
    uint8_t  bitDepthChromaMinus8;
    uint16_t avgFrameRate;
    uint8_t  constantFrameRate;
    uint8_t  numTemporalLayers;
    uint8_t  temporalIdNested;
    uint8_t  lengthSizeMinusOne;
    std::vector<std::vector<uint8_t>> sps;
    std::vector<std::vector<uint8_t>> pps;
    std::vector<std::vector<uint8_t>> vps;
};

} // namespace nx::media::hevc
