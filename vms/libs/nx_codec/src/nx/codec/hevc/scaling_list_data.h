// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/bit_stream.h>

namespace nx::media::hevc {

struct NX_CODEC_API ScalingListData
{
    bool read(nx::utils::BitStreamReader& reader);
    struct ScalingListElement
    {
        uint8_t scaling_list_pred_mode_flag;
        uint8_t scaling_list_pred_matrix_id_delta;
        int16_t scaling_list_dc_coef_minus8;
        int8_t scaling_list_delta_coef[64];

        uint32_t coefNum = 0;
    };

    ScalingListElement data[4][6];
};

} // namespace nx::media::hevc
