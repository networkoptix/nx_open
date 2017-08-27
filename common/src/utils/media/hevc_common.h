#pragma once

namespace nx {
namespace media_utils {
namespace hevc {

const uint8_t kNalUnitPrefix[] = {0x00, 0x00, 0x00, 0x01};
const uint8_t kShortNalUnitPrefix[] = {0x00, 0x00, 0x01};

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

struct FuHeader
{
    bool startFlag = 0;
    bool endFlag = 0;
    NalUnitType unitType = NalUnitType::unspec63;

    const static int kTotalLength = 1;

    bool decode(const uint8_t* const payload, int payloadLength);
};

struct NalUnitHeader
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

    bool decode(const uint8_t* const payload, int payloadLength);
};

PacketType fromNalUnitTypeToPacketType(NalUnitType unitType);
bool isValidUnitType(int unitType);
bool isRandomAccessPoint(NalUnitType unitType);
bool isLeadingPicture(NalUnitType unitType);
bool isTrailingPicture(NalUnitType unitType);
bool isParameterSet(NalUnitType unitType);

enum class Profile
{
    undefined,
    // version 1
    main,
    main10,
    mainStillPicture,

    // version 2
    monochrome,
    monochrome12,
    monochrome16,
    main12,
    main42210,
    main42212,
    main444,
    main44410,
    main44412,
    main44416Intra,
    highThroughput44416Intra,
    main444StillPicture,
    main44416StillPicture,
    scalableMain,
    scalableMain10,
    multiviewMain,

    // version 3
    threeDMain, //< 3D Main
    screenExtendedMain,
    screenExtendedMain10,
    screenExtendedMain444,
    screenExtendedMain44410,
    screenExtendedHighThroughput444,
    screenExtendedHighThroughput44410,
    screenExtendedHighThroughput44414,
    highThroughput444,
    highThroughput44410,
    highThroughput44414,
    scalableMonochrome,
    scalableMonochrome12,
    scalableMonochrome16,
    scalableMain444
};

} // namespace hevc
} // namespace media_utils
} // namespace nx

