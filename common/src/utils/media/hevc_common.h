#pragma once

namespace nx {
namespace media_utils {
namespace hevc {

const uint8_t kNalUnitPrefix[] = {0x00, 0x00, 0x00, 0x01};
const uint8_t kShortNalUnitPrefix[] = {0x00, 0x00, 0x01};

enum class NalUnitType
{
    TrailN = 0, TrailR, // Trailing pictures

    TsaN, TsaR, StsaN, StsaR, // Temporal sublayer access

    RadlN, RadlR, RaslN, RaslR, // Leading pictures

    RsvVclN10, RsvVclR11, RsvVclN12, RsvVclR13, RsvVclN14, RsvVclR15,

    BlaWLp, BlaWRadl, BlaNlp, IdrWRadl, IdrNLp, CraNut, // IRAPs

    RsvIrapVcl22, RsvIrapVcl23,
    RsvVcl24, RsvVcl25, RsvVcl26, RsvVcl27, RsvVcl28, RsvVcl29, RsvVcl30, RsvVcl31,

    // Different stuff
    VpsNut, SpsNut, PpsNut, AudNut, EosNut, EobNut, FdNut, PrefixSeiNut, SuffixSeiNut,

    RsvNvcl41, RsvNvcl42, RsvNvcl43, RsvNvcl44, RsvNvcl45, RsvNvcl46, RsvNvcl47,
    Unspec48, Unspec49, Unspec50, Unspec51, Unspec52, Unspec53, Unspec54, Unspec55,
    Unspec56, Unspec57, Unspec58, Unspec59, Unspec60, Unspec61, Unspec62, Unspec63,
};

enum class PacketType
{
    AggregationPacket = 48,
    FragmentationPacket = 49,
    PaciPacket = 50,
    SingleNalUnitPacket = 64,
};

struct FuHeader
{
    bool startFlag = 0;
    bool endFlag = 0;
    NalUnitType unitType;

    const static int kTotalLength = 1;

    bool decode(const uint8_t* const payload, int payloadLength);
};

struct NalUnitHeader
{
    NalUnitType unitType = NalUnitType::Unspec63;
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

} // namespace hevc
} // namespace media_utils
} // namespace nx