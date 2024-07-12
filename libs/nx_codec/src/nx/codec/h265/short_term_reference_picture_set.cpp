// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "short_term_reference_picture_set.h"

namespace nx::media::h265 {

bool ShortTermReferencePictureSet::read(
    nx::utils::BitStreamReader& reader,
    uint32_t stRpsIdx,
    uint32_t max_dec_pic_buffering_minus1,
    uint32_t num_short_term_ref_pic_sets,
    const std::vector<ShortTermReferencePictureSet>& spsSets)
{
    if (stRpsIdx != 0)
        inter_ref_pic_set_prediction_flag = reader.getBits(1);
    else
        inter_ref_pic_set_prediction_flag = 0;

    if (inter_ref_pic_set_prediction_flag)
    {
        // read bitstream data
        if (stRpsIdx == num_short_term_ref_pic_sets)
            delta_idx_minus1 = reader.getGolomb();
        else
            delta_idx_minus1 = 0;

        uint32_t refRpsIdx = stRpsIdx - (delta_idx_minus1 + 1);

        if (refRpsIdx >= spsSets.size())
            return false;

        delta_rps_sign = reader.getBits(1);
        abs_delta_rps_minus1 = reader.getGolomb();
        int32_t deltaRps = (1 - 2 * (signed)(delta_rps_sign)) * ((signed)(abs_delta_rps_minus1) + 1);

        used_by_curr_pic_flags.resize(spsSets[refRpsIdx].NumDeltaPocs + 1);
        use_delta_flags.assign(spsSets[refRpsIdx].NumDeltaPocs + 1, 1); // when absent, inferred to 1
        for (uint32_t j = 0; j <= spsSets[refRpsIdx].NumDeltaPocs; ++j)
        {
            used_by_curr_pic_flags[j] = reader.getBits(1);
            if (!used_by_curr_pic_flags[j])
                use_delta_flags[j] = reader.getBits(1);

        }

        // compute higher order variables
        computeRefNegativeVariables(spsSets[refRpsIdx], deltaRps);
        computeRefPositiveVariables(spsSets[refRpsIdx], deltaRps);
        NumDeltaPocs = NumNegativePics + NumPositivePics;
    }
    else
    {
        // read bitstream data
        num_negative_pics = reader.getGolomb();
        if (num_negative_pics > max_dec_pic_buffering_minus1)
            return false;

        num_positive_pics = reader.getGolomb();
        if (num_positive_pics > max_dec_pic_buffering_minus1 - num_negative_pics)
            return false;

        delta_poc_s0_minus1_vector.resize(num_negative_pics);
        used_by_curr_pic_s0_flags.resize(num_negative_pics);
        for (uint32_t i = 0; i < num_negative_pics; ++i)
        {
            delta_poc_s0_minus1_vector[i] = reader.getGolomb();
            used_by_curr_pic_s0_flags[i] = reader.getBits(1);
        }

        delta_poc_s1_minus1_vector.resize(num_positive_pics);
        used_by_curr_pic_s1_flags.resize(num_positive_pics);
        for (uint32_t i = 0; i < num_positive_pics; ++i)
        {
            delta_poc_s1_minus1_vector[i] = reader.getGolomb();
            used_by_curr_pic_s1_flags[i] = reader.getBits(1);
        }

        // compute higher order variables
        NumNegativePics = num_negative_pics;
        NumPositivePics = num_positive_pics;
        NumDeltaPocs = NumNegativePics + NumPositivePics;
        UsedByCurrPicS0 = used_by_curr_pic_s0_flags;
        UsedByCurrPicS1 = used_by_curr_pic_s1_flags;

        DeltaPocS0.resize(num_negative_pics);
        for (uint32_t i = 0; i < DeltaPocS0.size(); ++i)
        {
            if (i == 0)
                DeltaPocS0[i] = -(delta_poc_s0_minus1_vector[i] + 1);
            else
                DeltaPocS0[i] = delta_poc_s0_minus1_vector[i - 1] - (delta_poc_s0_minus1_vector[i] + 1);
        }

        DeltaPocS1.resize(num_positive_pics);
        for (uint32_t i = 0; i < DeltaPocS1.size(); ++i)
        {
            if (i == 0)
                DeltaPocS1[i] = delta_poc_s1_minus1_vector[i] + 1;
            else
                DeltaPocS1[i] = delta_poc_s1_minus1_vector[i - 1] + (delta_poc_s1_minus1_vector[i] + 1);

        }
    }

    return true;
}

void ShortTermReferencePictureSet::computeRefNegativeVariables(
    const ShortTermReferencePictureSet& refSet, int32_t deltaRps)
{
    uint32_t i = 0;
    DeltaPocS0.clear();
    UsedByCurrPicS0.clear();
    for (int32_t j = refSet.NumPositivePics - 1; j >= 0; --j)
    {
        int32_t dPoc = (signed)(refSet.DeltaPocS1[j]) + deltaRps;
        if (dPoc < 0 && use_delta_flags[refSet.NumNegativePics + j])
        {
            DeltaPocS0.push_back(dPoc);
            UsedByCurrPicS0.push_back(used_by_curr_pic_flags[refSet.NumNegativePics + j]);
            ++i;
        }
    }

    if (deltaRps < 0 && use_delta_flags[refSet.NumDeltaPocs])
    {
        DeltaPocS0.push_back(deltaRps);
        UsedByCurrPicS0.push_back(used_by_curr_pic_flags[refSet.NumDeltaPocs]);
        ++ i;
    }

    for(uint32_t j = 0; j < refSet.NumNegativePics; ++j)
    {
        int32_t dPoc = (signed)(refSet.DeltaPocS0[j]) + deltaRps;
        if (dPoc < 0 && use_delta_flags[j])
        {
            DeltaPocS0.push_back(dPoc);
            UsedByCurrPicS0.push_back(used_by_curr_pic_flags[j]);
            ++i;
        }
    }

    NumNegativePics = i;
}

void ShortTermReferencePictureSet::computeRefPositiveVariables(
    const ShortTermReferencePictureSet& refSet, int32_t deltaRps)\
{
    uint32_t i = 0;

    DeltaPocS1.clear();
    UsedByCurrPicS1.clear();
    for (int32_t j = refSet.NumNegativePics - 1; j >= 0; --j)
    {
        int32_t dPoc = (signed)(refSet.DeltaPocS0[j]) + deltaRps;
        if (dPoc > 0 && use_delta_flags[j])
        {
            DeltaPocS1.push_back(dPoc);
            UsedByCurrPicS1.push_back(used_by_curr_pic_flags[j]);
            ++i;
        }
    }

    if (deltaRps > 0 && use_delta_flags[refSet.NumDeltaPocs])
    {
        DeltaPocS1.push_back(deltaRps);
        UsedByCurrPicS1.push_back(used_by_curr_pic_flags[refSet.NumDeltaPocs]);
        ++i;
    }

    for (uint32_t j = 0; j < refSet.NumPositivePics; ++j)
    {
        int32_t dPoc = signed(refSet.DeltaPocS1[j]) + deltaRps;
        if (dPoc > 0 && use_delta_flags[refSet.NumNegativePics + j])
        {
            DeltaPocS1.push_back(dPoc);
            UsedByCurrPicS1.push_back(used_by_curr_pic_flags[refSet.NumNegativePics + j]);
            ++i;
        }
    }

    NumPositivePics = i;
}

} // namespace nx::media::h265
