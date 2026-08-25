// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

// See https://aomediacodec.github.io/av1-spec/av1-spec.pdf

namespace nx::media::av1 {

enum class ObuType
{
    reserved0 = 0,
    sequenceHeader = 1,
    temporalDelimiter = 2,
    frameHeader = 3,
    tileGroup = 4,
    metadata = 5,
    frame = 6,
    redundantFrameHeader = 7,
    tileList = 8,
    padding = 15,
};

/** leb128() values are at most 8 bytes long, see the spec, 4.10.5. */
static constexpr int kMaxLeb128Bytes = 8;

/** obu_type = TEMPORAL_DELIMITER, obu_has_size_field = 1, obu_size = 0. */
static constexpr uint8_t kTemporalDelimiterObu[] = {0x12, 0x00};

/**
 * Read a leb128() value. Non minimal encodings are accepted.
 * @return Number of bytes read (1..kMaxLeb128Bytes), or 0 if the data is truncated, the encoding
 *     is longer than kMaxLeb128Bytes or the value does not fit into 32 bits.
 */
NX_CODEC_API int readLeb128(const uint8_t* data, int size, uint64_t* outValue);

/**
 * Write the minimal leb128() encoding of the value. The buffer must have at least
 * kMaxLeb128Bytes bytes.
 * @return Number of bytes written.
 */
NX_CODEC_API int writeLeb128(uint64_t value, uint8_t* buffer);

struct NX_CODEC_API ObuHeader
{
    static constexpr int kMaxSize = 2;

    ObuType type = ObuType::reserved0;
    bool hasExtension = false;
    bool hasSizeField = false;
    uint8_t temporalId = 0;
    uint8_t spatialId = 0;
    uint8_t extensionReserved = 0; //< Kept to re-encode the header without changing reserved bits.
    uint8_t headerReserved = 0; //< obu_reserved_1bit.

    /** @return Header size (1 or 2), or 0 if the data is truncated or obu_forbidden_bit is set. */
    int decode(const uint8_t* data, int size);

    /** The buffer must have at least kMaxSize bytes. @return Number of bytes written. */
    int encode(uint8_t* buffer, bool withSizeField) const;
};

} // namespace nx::media::av1
