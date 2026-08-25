// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

namespace nx::media::av1 {

/**
 * Sequence header OBU, see the AV1 spec, 5.5.1. Only the part needed to get the frame size and
 * the profile/level/tier is parsed; everything up to and including frame_id_numbers_present_flag
 * is decoded because the fields have data dependent sizes and can not be skipped.
 */
struct NX_CODEC_API SequenceHeader
{
    uint8_t seqProfile = 0;
    bool stillPicture = false;
    bool reducedStillPictureHeader = false;
    uint8_t seqLevelIdx0 = 0; //< Operating point 0.
    uint8_t seqTier0 = 0; //< Operating point 0.
    int maxFrameWidth = 0; //< max_frame_width_minus_1 + 1.
    int maxFrameHeight = 0; //< max_frame_height_minus_1 + 1.
    bool frameIdNumbersPresentFlag = false;

    /**
     * @param data Sequence header OBU payload, i.e. after obu_header and after obu_size.
     */
    bool read(const uint8_t* data, int size);
};

} // namespace nx::media::av1
