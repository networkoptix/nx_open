// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hevc_common.h"

#include <QtGlobal>

#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace nx::media::h265 {

namespace {

static const uint8_t kPayloadHeaderForbiddenZeroBitMask = 0x80;
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    static const uint16_t kPayloadHeaderLayerIdMask = 0x1f8;
#else
    static const uint16_t kPayloadHeaderLayerIdMask = 0xf801;
#endif
static const uint8_t kPayloadHeaderTidMask = 0x07;
static const uint8_t kStartFuPacketMask = 0x80;
static const uint8_t kEndFuPacketMask = 0x40;
static const uint8_t kFuNalUnitTypeMask = 0x3f;

} // namespace

bool FuHeader::decode(const uint8_t* const payload, int payloadLength)
{
    if (payloadLength < 1)
        return false;

    startFlag = *payload & kStartFuPacketMask;
    endFlag = *payload & kEndFuPacketMask;
    unitType = (NalUnitType)(*payload & kFuNalUnitTypeMask);

    return true;
}

bool NalUnitHeader::decode(const uint8_t* const payload, int payloadLength, QString* outErrorString)
{
    if (outErrorString)
        outErrorString->clear();

    if (payloadLength < kTotalLength)
    {
        if (outErrorString)
            *outErrorString = NX_FMT("Payload length is too short: %1 byte(s)", payloadLength);
        return false;
    }

    if (payload[0] & kPayloadHeaderForbiddenZeroBitMask)
        NX_WARNING(this, "Incorrect hevc nal unit header, forbidden zero bit is set!");

    unitType = (NalUnitType)((payload[0] & kPayloadHeaderNalUnitTypeMask) >> 1);
    layerId = ((*((uint16_t*)payload) & kPayloadHeaderLayerIdMask) >> kTidFieldLengthBit);
    tid = payload[1] & kPayloadHeaderTidMask;

    return true;
}

PacketType fromNalUnitTypeToPacketType(NalUnitType unitType)
{
    switch (unitType)
    {
        case NalUnitType::unspec48:
            return PacketType::aggregationPacket;
        case NalUnitType::unspec49:
            return PacketType::fragmentationPacket;
        case NalUnitType::unspec50:
            return PacketType::paciPacket;
        default:
            return PacketType::singleNalUnitPacket;
    }
}

bool isValidUnitType(int unitType)
{
    return unitType >= (int)NalUnitType::trailN
        && unitType <= (int)NalUnitType::unspec63;
}

bool isRandomAccessPoint(NalUnitType unitType)
{
    return unitType >= NalUnitType::blaWLp
        && unitType <= NalUnitType::craNut;
}

bool isLeadingPicture(NalUnitType unitType)
{
    return unitType >= NalUnitType::radlN
        && unitType <= NalUnitType::raslR;
}

bool isTrailingPicture(NalUnitType unitType)
{
    return unitType == NalUnitType::trailN
        || unitType == NalUnitType::trailR;
}

bool isParameterSet(NalUnitType unitType)
{
    return unitType == NalUnitType::vpsNut
        || unitType == NalUnitType::spsNut
        || unitType == NalUnitType::ppsNut;
}

bool isSlice(NalUnitType unitType)
{
    return (unitType >= NalUnitType::trailN && unitType <= NalUnitType::raslR)
        || (unitType >= NalUnitType::blaWLp && unitType <= NalUnitType::rsvIrapVcl23);
}

bool isNewAccessUnit(NalUnitType unitType)
{
    return (unitType >= NalUnitType::vpsNut && unitType <= NalUnitType::eobNut)
        || (unitType == NalUnitType::prefixSeiNut)
        || (unitType >= NalUnitType::rsvNvcl41 && unitType <= NalUnitType::rsvNvcl44)
        || (unitType >= NalUnitType::unspec48 && unitType <= NalUnitType::unspec55);
}

} // namespace nx::media::h265
