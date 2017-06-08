#include "nal_unit_h265.h"

#include <utils/media/bitStream.h>

namespace nx {
namespace media_utils {
namespace hevc {

namespace {

static const int kOctetLength = 8;

} // namespace 

bool isValidUnitType(NalUnitType unitType)
{
    unitType >= NalUnitType::TrailN
        && unitType <= NalUnitType::UnspecEnd;
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

bool NalUnitHeader::decodeFromPayload(const uint8_t* const payload, int payloadLength)
{
    if (payloadLength < kTotalLength / kOctetLength)
        return false;

    BitStreamReader reader(payload, payloadLength);
    
    if (reader.getBit()) //< forbidden zero bit
        return false;

    unitType = (NalUnitType)reader.getBits(kUnitTypeFieldLength);
    if (!isValidUnitType(unitType))
        return false;

    layerId = reader.getBits(kLayerIdFieldLength);
    if (layerId) //< Should always be zero
        return false;

    tid = reader.getBits(kTidFieldLength);
    return true;
}

} // namespace hevc
} // namespace media_utils
} // namespace nx
