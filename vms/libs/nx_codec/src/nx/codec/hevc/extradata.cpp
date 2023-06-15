// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "extradata.h"

#include <nx/codec/hevc/hevc_common.h>
#include <nx/codec/hevc/hevc_decoder_configuration_record.h>
#include <nx/codec/hevc/picture_parameter_set.h>
#include <nx/codec/hevc/sequence_parameter_set.h>
#include <nx/codec/hevc/video_parameter_set.h>
#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace {

static constexpr uint16_t kMaxSpatialSefmentation = 4096;
static constexpr uint8_t kNaluLengthSizeBytes = 4;

using namespace nx::media::hevc;

void updatePTL(const ProfileTierLevel& ptl, HEVCDecoderConfigurationRecord& hvcc)
{
    hvcc.general_profile_space = ptl.general.profile_space;
    if (!hvcc.general_tier_flag && ptl.general.tier_flag) {
        hvcc.general_level_idc = ptl.general.level_idc;
    } else {
        hvcc.general_level_idc = std::max<uint8_t>(hvcc.general_level_idc, ptl.general.level_idc);
    }
    hvcc.general_tier_flag = hvcc.general_tier_flag || ptl.general.tier_flag;
    hvcc.general_profile_idc = std::max<uint8_t>(hvcc.general_profile_idc, ptl.general.profile_idc);
    hvcc.general_profile_compatibility_flags &= ptl.general.profile_compatibility_flags.to_ulong();
    hvcc.general_constraint_indicator_flags &= ptl.general.constraint_indicator_flags();
}

}

namespace nx::media::hevc {

std::vector<uint8_t> buildExtraDataAnnexB(const uint8_t* data, int32_t size)
{
    std::vector<uint8_t> extraData;
    std::vector<uint8_t> startcode = {0x0, 0x0, 0x0, 0x1};
    auto nalUnits = nx::media::nal::findNalUnitsAnnexB(data, size);
    for (const auto& nalu: nalUnits)
    {
        NalUnitType unitType = (NalUnitType)((*nalu.data & kPayloadHeaderNalUnitTypeMask) >> 1);
        if (unitType == NalUnitType::vpsNut
            || unitType == NalUnitType::ppsNut
            || unitType == NalUnitType::spsNut)
        {
            extraData.insert(extraData.end(), startcode.begin(), startcode.end());
            extraData.insert(extraData.end(), nalu.data, nalu.data + nalu.size);
        }
    }
    return extraData;
}

std::vector<uint8_t> buildExtraDataMp4FromAnnexB(const uint8_t* data, int32_t size)
{
    return buildExtraDataMp4(nal::findNalUnitsAnnexB(data, size));
}

std::vector<uint8_t> buildExtraDataMp4(const std::vector<nal::NalUnitInfo>& nalUnits)
{
    HEVCDecoderConfigurationRecord hvcc;
    for (const auto& nalu: nalUnits)
    {
        NalUnitType unitType = (NalUnitType)((*nalu.data & kPayloadHeaderNalUnitTypeMask) >> 1);
        if (unitType == NalUnitType::vpsNut)
            hvcc.vps.push_back(std::vector<uint8_t>(nalu.data, nalu.data + nalu.size));
        if (unitType == NalUnitType::ppsNut)
            hvcc.pps.push_back(std::vector<uint8_t>(nalu.data, nalu.data + nalu.size));
        if (unitType == NalUnitType::spsNut)
            hvcc.sps.push_back(std::vector<uint8_t>(nalu.data, nalu.data + nalu.size));
    }

    if (hvcc.sps.empty() || hvcc.pps.empty() || hvcc.vps.empty())
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to write h264 extra data, no sps/pps/vps found");
        return std::vector<uint8_t>();
    }
    SequenceParameterSet sps;
    if (!parseNalUnit(hvcc.sps[0].data(), hvcc.sps[0].size(), sps))
        return std::vector<uint8_t>();

    PictureParameterSet pps;
    if (!parseNalUnit(hvcc.pps[0].data(), hvcc.pps[0].size(), pps))
        return std::vector<uint8_t>();

    VideoParameterSet vps;
    if (!parseNalUnit(hvcc.vps[0].data(), hvcc.vps[0].size(), vps))
        return std::vector<uint8_t>();

    hvcc.lengthSizeMinusOne = kNaluLengthSizeBytes - 1;
    hvcc.configurationVersion = 1;
    hvcc.general_tier_flag = false;
    hvcc.general_level_idc = 0;
    hvcc.general_profile_idc = 0;
    hvcc.general_profile_compatibility_flags = 0xffffffff;
    hvcc.general_constraint_indicator_flags  = 0xffffffffffff;
    hvcc.min_spatial_segmentation_idc = kMaxSpatialSefmentation + 1;
    hvcc.avgFrameRate = 0;
    hvcc.constantFrameRate = 0;
    hvcc.numTemporalLayers = 0;
    hvcc.lengthSizeMinusOne = 3;

    // VPS
    updatePTL(vps.profile_tier_level, hvcc);
    hvcc.numTemporalLayers = std::max<uint8_t>(
        hvcc.numTemporalLayers, vps.vps_max_sub_layers_minus1 + 1);

    // SPS
    updatePTL(sps.profile_tier_level, hvcc);
    hvcc.min_spatial_segmentation_idc = std::min<uint16_t>(
        hvcc.min_spatial_segmentation_idc, sps.vui_parameters.min_spatial_segmentation_idc);
    hvcc.chromaFormat = sps.chroma_format_idc;
    hvcc.bitDepthLumaMinus8 = sps.bit_depth_luma_minus8;
    hvcc.bitDepthChromaMinus8 = sps.bit_depth_chroma_minus8;
    hvcc.numTemporalLayers = std::max<uint8_t>(
        hvcc.numTemporalLayers, sps.sps_max_sub_layers_minus1 + 1);
    hvcc.temporalIdNested = sps.sps_temporal_id_nesting_flag;

    //PPS
    if (pps.entropy_coding_sync_enabled_flag && pps.tiles_enabled_flag)
        hvcc.parallelismType = 0; // mixed-type parallel decoding
    else if (pps.entropy_coding_sync_enabled_flag)
        hvcc.parallelismType = 3; // wavefront-based parallel decoding
        else if (pps.tiles_enabled_flag)
        hvcc.parallelismType = 2; // tile-based parallel decoding
    else
        hvcc.parallelismType = 1;

    if (hvcc.min_spatial_segmentation_idc > kMaxSpatialSefmentation)
        hvcc.min_spatial_segmentation_idc = 0;

    std::vector<uint8_t> extradata(hvcc.size());
    hvcc.write(extradata.data(), extradata.size());
    return extradata;
}

} // namespace nx::media::hevc
