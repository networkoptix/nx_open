#include "hevc_common.h"

#include <QtGlobal>

namespace nx {
namespace media_utils {
namespace hevc {

namespace {

static const uint8_t kPayloadHeaderForbiddenZeroBitMask = 0x80;
static const uint8_t kPayloadHeaderNalUnitTypeMask = 0x7e;
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

} // namespace hevc
} // namespace media_utils
} // namespace nx
