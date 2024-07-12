// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <vector>

#include <QtCore/QString>

#include <nx/codec/nal_units.h>

namespace nx::media::h265 {

static const uint8_t kPayloadHeaderNalUnitTypeMask = 0x7e;

enum class NalUnitType
{
    trailN = 0, trailR, // Trailing pictures

    tsaN, tsaR, stsaN, stsaR, // Temporal sublayer access

    radlN, radlR, raslN, raslR, // Leading pictures

    rsvVclN10, rsvVclR11, rsvVclN12, rsvVclR13, rsvVclN14, rsvVclR15,

    blaWLp, blaWRadl, blaNlp, idrWRadl, idrNLp, craNut, // IRAPs

    rsvIrapVcl22, rsvIrapVcl23,
    rsvVcl24, rsvVcl25, rsvVcl26, rsvVcl27, rsvVcl28, rsvVcl29, rsvVcl30, rsvVcl31,

    // Different stuff
    vpsNut, spsNut, ppsNut, audNut, eosNut, eobNut, fdNut, prefixSeiNut, suffixSeiNut,

    rsvNvcl41, rsvNvcl42, rsvNvcl43, rsvNvcl44, rsvNvcl45, rsvNvcl46, rsvNvcl47,
    unspec48, unspec49, unspec50, unspec51, unspec52, unspec53, unspec54, unspec55,
    unspec56, unspec57, unspec58, unspec59, unspec60, unspec61, unspec62, unspec63,
};

enum class PacketType
{
    aggregationPacket = 48,
    fragmentationPacket = 49,
    paciPacket = 50,
    singleNalUnitPacket = 64,
};

struct NX_CODEC_API FuHeader
{
    bool startFlag = 0;
    bool endFlag = 0;
    NalUnitType unitType = NalUnitType::unspec63;

    const static int kTotalLength = 1;

    bool decode(const uint8_t* const payload, int payloadLength);
};

struct NX_CODEC_API NalUnitHeader
{
    NalUnitType unitType = NalUnitType::unspec63;
    int layerId = 0;
    int tid = 0; // TemporalId + 1

    static const int kForbiddenFieldLengthBit = 1;
    static const int kUnitTypeFieldLengthBit = 6;
    static const int kLayerIdFieldLengthBit = 6;
    static const int kTidFieldLengthBit = 3;

    static const int kTotalLengthBit = 16;
    static const int kTotalLength = 2;

    bool decode(const uint8_t* const payload, int payloadLength, QString* outErrorString = nullptr);
};

template<typename NalUnitType>
bool parseNalUnit(const uint8_t* data, int size, NalUnitType& result)
{
    if (size <= NalUnitHeader::kTotalLength)
        return false;

    data += NalUnitHeader::kTotalLength;
    size -= NalUnitHeader::kTotalLength;

    // Remove emulation_prevention_three_byte.
    std::vector<uint8_t> decodedBuffer(size);
    auto decodedData = decodedBuffer.data();

    int decodedSize = nx::media::nal::decodeEpb(data, data + size, decodedData, size);
    return result.read(decodedData, decodedSize);
}

NX_CODEC_API PacketType fromNalUnitTypeToPacketType(NalUnitType unitType);
bool isValidUnitType(int unitType);
NX_CODEC_API bool isRandomAccessPoint(NalUnitType unitType);
bool isLeadingPicture(NalUnitType unitType);
bool isTrailingPicture(NalUnitType unitType);
bool isParameterSet(NalUnitType unitType);
NX_CODEC_API bool isSlice(NalUnitType unitType);
NX_CODEC_API bool isNewAccessUnit(NalUnitType unitType); //< check is new AU, excluding slices

} // namespace nx::media::h265
