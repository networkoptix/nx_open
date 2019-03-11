#pragma once

#include <cstdint>

namespace nx::media_utils::h263 {

enum class PictureType
{
    IPicture,
    PPicture,
    PBFrame,
    BPicture,
    EIPicture,
    EPPicture,
    Reserved,
    Reserved1,
};

enum class Format
{
    Forbidden = 0,
    SubQCif = 1,
    QCif = 2,
    Cif = 3,
    Cif4 = 4,
    Cif16 = 5,
    Reserved = 6,
    ExtendedPType = 7,
};

inline bool isKeyFrame(PictureType pictureType)
{
    return pictureType == PictureType::IPicture || pictureType == PictureType::Reserved1;
}

// Implemented 'pictureType' parsing only!
struct PictureHeader
{
    bool decode(const uint8_t* data, uint32_t size);

    PictureType pictureType = PictureType::IPicture;
    uint32_t timestamp = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    Format format = Format::Forbidden;
};

} // nx::media_utils::h263
