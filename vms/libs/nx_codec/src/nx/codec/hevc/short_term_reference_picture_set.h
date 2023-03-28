// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/bit_stream.h>

namespace nx::media::hevc {

struct NX_CODEC_API ShortTermReferencePictureSet
{
    bool read(nx::utils::BitStreamReader& reader,
        uint32_t stRpsIdx,
        uint32_t max_dec_pic_buffering_minus1,
        uint32_t num_short_term_ref_pic_sets,
        const std::vector<ShortTermReferencePictureSet>& spsSets);

        uint8_t inter_ref_pic_set_prediction_flag;

    // apply if inter_ref_pic_set_prediction_flag is set
    uint32_t delta_idx_minus1;
    uint8_t delta_rps_sign;
    uint32_t abs_delta_rps_minus1;

    std::vector<uint8_t> used_by_curr_pic_flags;
    std::vector<uint8_t> use_delta_flags;

    // apply if inter_ref_pic_set_prediction_flag is not set
    uint32_t num_negative_pics;
    uint32_t num_positive_pics;

    std::vector<uint32_t> delta_poc_s0_minus1_vector;
    std::vector<uint8_t> used_by_curr_pic_s0_flags;

    std::vector<uint32_t> delta_poc_s1_minus1_vector;
    std::vector<uint8_t> used_by_curr_pic_s1_flags;

    // higher order variables, not raw bitstream data
    std::vector<uint8_t> UsedByCurrPicS0;
    std::vector<uint8_t> UsedByCurrPicS1;
    std::vector<int32_t> DeltaPocS0;
    std::vector<int32_t> DeltaPocS1;
    uint32_t NumNegativePics = 0;
    uint32_t NumPositivePics = 0;
    uint32_t NumDeltaPocs = 0;

private:
    void computeRefNegativeVariables(const ShortTermReferencePictureSet& refSet, int32_t deltaRps);
    void computeRefPositiveVariables(const ShortTermReferencePictureSet& refSet, int32_t deltaRps);
};

} // namespace nx::media::hevc
