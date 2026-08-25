// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av1_common.h"

namespace nx::media::av1 {

namespace {

static constexpr uint8_t kObuForbiddenBitMask = 0x80;
static constexpr uint8_t kObuTypeMask = 0x78;
static constexpr uint8_t kObuExtensionFlagMask = 0x04;
static constexpr uint8_t kObuHasSizeFieldMask = 0x02;
static constexpr uint8_t kObuReservedBitMask = 0x01;

static constexpr uint8_t kExtensionTemporalIdMask = 0xe0;
static constexpr uint8_t kExtensionSpatialIdMask = 0x18;
static constexpr uint8_t kExtensionReservedMask = 0x07;

static constexpr uint8_t kLeb128ContinuationMask = 0x80;
static constexpr uint8_t kLeb128ValueMask = 0x7f;

static constexpr uint64_t kMaxLeb128Value = 0xffffffff;

} // namespace

int readLeb128(const uint8_t* data, int size, uint64_t* outValue)
{
    uint64_t value = 0;
    for (int i = 0; i < kMaxLeb128Bytes; ++i)
    {
        if (i >= size)
            return 0; //< Truncated.

        value |= ((uint64_t) (data[i] & kLeb128ValueMask)) << (i * 7);
        if (!(data[i] & kLeb128ContinuationMask))
        {
            if (value > kMaxLeb128Value)
                return 0;

            *outValue = value;
            return i + 1;
        }
    }
    return 0; //< The encoding is longer than kMaxLeb128Bytes.
}

int writeLeb128(uint64_t value, uint8_t* buffer)
{
    int result = 0;
    do
    {
        uint8_t byte = value & kLeb128ValueMask;
        value >>= 7;
        if (value != 0)
            byte |= kLeb128ContinuationMask;
        buffer[result++] = byte;
    } while (value != 0);
    return result;
}

int ObuHeader::decode(const uint8_t* data, int size)
{
    if (size < 1 || (data[0] & kObuForbiddenBitMask))
        return 0;

    type = (ObuType) ((data[0] & kObuTypeMask) >> 3);
    hasExtension = data[0] & kObuExtensionFlagMask;
    hasSizeField = data[0] & kObuHasSizeFieldMask;
    headerReserved = data[0] & kObuReservedBitMask;

    if (!hasExtension)
        return 1;

    if (size < kMaxSize)
        return 0;

    temporalId = (data[1] & kExtensionTemporalIdMask) >> 5;
    spatialId = (data[1] & kExtensionSpatialIdMask) >> 3;
    extensionReserved = data[1] & kExtensionReservedMask;
    return kMaxSize;
}

int ObuHeader::encode(uint8_t* buffer, bool withSizeField) const
{
    buffer[0] = (((uint8_t) type) << 3) & kObuTypeMask;
    if (hasExtension)
        buffer[0] |= kObuExtensionFlagMask;
    if (withSizeField)
        buffer[0] |= kObuHasSizeFieldMask;
    buffer[0] |= headerReserved & kObuReservedBitMask;

    if (!hasExtension)
        return 1;

    buffer[1] =
        (temporalId << 5) | (spatialId << 3) | (extensionReserved & kExtensionReservedMask);
    return kMaxSize;
}

} // namespace nx::media::av1
