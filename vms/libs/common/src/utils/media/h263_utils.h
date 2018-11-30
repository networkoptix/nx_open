#pragma once

#include <cstdint>

namespace nx::media_utils::h263 {

enum class PictureType
{
    I,
    P,
    B,
};

// Implemented 'pictureType' parsing only!
struct PictureHeader
{
    bool decode(const uint8_t* data, uint32_t size);

    PictureType pictureType = PictureType::I;
    uint32_t timestamp = 0;
    /**
     * 0  forbidden
     * 1  sub-QCIF
     * 10 QCIF
     * 7  extended PTYPE (PLUSPTYPE)
     */
    uint8_t format = 0;
};

} // nx::media_utils::h263
