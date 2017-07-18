#include "hevc_common.h"

namespace nx {
namespace media_utils {
namespace hevc {

namespace {

static const uint8_t kPayloadHeaderForbiddenZeroBitMask = 0x80;
static const uint8_t kPayloadHeaderNalUnitTypeMask = 0x7e;
static const uint16_t kPayloadHeaderLayerIdMask = htons(0x1f8);
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

bool NalUnitHeader::decode(const uint8_t* const payload, int payloadLength)
{
    if (payloadLength < kTotalLength)
        return false;

    if (payload[0] & kPayloadHeaderForbiddenZeroBitMask)
        return false;

    unitType = (NalUnitType)((payload[0] & kPayloadHeaderNalUnitTypeMask) >> 1);
    layerId = ((*((uint16_t*)payload) & kPayloadHeaderLayerIdMask) >> kTidFieldLengthBit);

    if (layerId) //< Should always be zero
        return false;

    tid = payload[1] & kPayloadHeaderTidMask;
    return true;
}

PacketType fromNalUnitTypeToPacketType(NalUnitType unitType)
{
    switch (unitType)
    {
        case NalUnitType::Unspec48:
            return PacketType::AggregationPacket;
        case NalUnitType::Unspec49:
            return PacketType::FragmentationPacket;
        case NalUnitType::Unspec50:
            return PacketType::PaciPacket;
        default:
            return PacketType::SingleNalUnitPacket;
    }
}

bool isValidUnitType(int unitType)
{
    return unitType >= (int)NalUnitType::TrailN
        && unitType <= (int)NalUnitType::Unspec63;
}

bool isRandomAccessPoint(NalUnitType unitType)
{
    return unitType >= NalUnitType::BlaWLp
        && unitType <= NalUnitType::CraNut;
}

bool isLeadingPicture(NalUnitType unitType)
{
    return unitType >= NalUnitType::RadlN
        && unitType <= NalUnitType::RaslR;
}

bool isTrailingPicture(NalUnitType unitType)
{
    return unitType == NalUnitType::TrailN
        || unitType == NalUnitType::TrailR;
}

bool isParameterSet(NalUnitType unitType)
{
    return unitType == NalUnitType::VpsNut
        || unitType == NalUnitType::SpsNut
        || unitType == NalUnitType::PpsNut;
}

} // namespace hevc
} // namespace media_utils
} // namespace nx
