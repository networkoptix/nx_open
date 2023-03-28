// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scaling_list_data.h"

namespace nx::media::hevc {

bool ScalingListData::read(nx::utils::BitStreamReader& reader)
{
    for (uint8_t sizeId = 0; sizeId < 4; ++ sizeId)
    {
        for (uint8_t matrixId = 0; matrixId < ((sizeId  ==  3) ? 2 : 6); ++ matrixId)
        {
            data[sizeId][matrixId].scaling_list_pred_mode_flag = reader.getBits(1);
            if (!data[sizeId][matrixId].scaling_list_pred_mode_flag)
            {
                data[sizeId][matrixId].scaling_list_pred_matrix_id_delta = reader.getGolomb();
                if (data[sizeId][matrixId].scaling_list_pred_matrix_id_delta > matrixId)
                    return false;
            }
            else
            {
                uint8_t coefNum = std::min(64, (1 << (4 + (sizeId << 1))));
                if (sizeId > 1)
                {
                    data[sizeId - 2][matrixId].scaling_list_dc_coef_minus8 = reader.getSignedGolomb();
                    data[sizeId][matrixId].scaling_list_dc_coef_minus8 = 8; // when not present, inferred to 8?...
                    if (data[sizeId - 2][matrixId].scaling_list_dc_coef_minus8 < -7 ||
                        data[sizeId - 2][matrixId].scaling_list_dc_coef_minus8 > 247)
                    {
                        return false;
                    }
                }

                for (uint8_t i = 0; i < coefNum; ++ i)
                    data[sizeId][matrixId].scaling_list_delta_coef[i] = reader.getSignedGolomb();

                data[sizeId][matrixId].coefNum = coefNum;
            }
        }
    }

    return true;
}

} // namespace nx::media::hevc
