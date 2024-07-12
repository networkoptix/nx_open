// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <inttypes.h>

#include <nx/utils/bit_stream.h>

namespace nx::media::nal {

// return pointer to nal_unit. If not found, 'end' is returned.
NX_CODEC_API const uint8_t* findNextNAL(const uint8_t* buffer, const uint8_t* end);
NX_CODEC_API const uint8_t* findNALWithStartCode(const uint8_t* buffer, const uint8_t* end);

// Encodes to emulation prevention three byte
NX_CODEC_API int encodeEpb(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize);

// Decodes from Annex B (removes emulation_prevention_three_byte)
// return Bytes written to \a dstBuffer
NX_CODEC_API int decodeEpb(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize);

struct NX_CODEC_API NalUnitInfo
{
    const uint8_t* data = nullptr;
    int size = 0;
};

NX_CODEC_API std::vector<NalUnitInfo> findNalUnitsAnnexB(
    const uint8_t* data, int32_t size, bool droppedFirstStartcode = false);

NX_CODEC_API std::vector<NalUnitInfo> findNalUnitsMp4(
    const uint8_t* data, int32_t size);

NX_CODEC_API std::vector<uint8_t> convertStartCodesToSizes(
    const uint8_t* data, int32_t size, int32_t padding = 0);

static const std::array<uint8_t, 4> kStartCode = { 0, 0, 0, 1 };
static const std::array<uint8_t, 3> kStartCode3B = { 0, 0, 1 };

static constexpr int kNalUnitSizeLength = 4;

NX_CODEC_API bool isStartCode(const void* data, size_t size);
NX_CODEC_API void convertToStartCodes(uint8_t* const data, const int size);
NX_CODEC_API void write_rbsp_trailing_bits(nx::utils::BitStreamWriter& writer);
NX_CODEC_API void write_byte_align_bits(nx::utils::BitStreamWriter& writer);
NX_CODEC_API QByteArray dropBorderedStartCodes(const QByteArray& sourceNal);

} // namespace nx::media::nal
