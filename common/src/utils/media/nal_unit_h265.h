#pragma once

namespace nx {
namespace media_utils {
namespace hevc {

enum class NalUnitType
{
    TrailN = 0,
    TrailR,
    TsaN,
    TsaR,
    StsaN,
    StsaR,
    RadlN,
    RadlR,
    RaslN,
    RaslR,
    RsvVclN10,
    RsvVclR11,
    RsvVclN12,
    RsvVclR13,
    RsvVclN14,
    RsvVclR15,
    BlaWLp,
    BlaWRadl,
    BlaNlp,
    IdrWRadl,
    IdrNLp,
    CraNut,
    RsvIrapVcl22,
    RsvIrapVcl23,
    RsvVclStart,
    VpsNut = 32,
    SpsNut,
    PpsNut,
    AudNut,
    EosNut,
    EobNut,
    FdNut,
    PrefixSeiNut,
    SuffixSeiNut,
    RsvNvclStart,
    UnspecStart = 48,
    UnspecEnd = 63
};

struct NalUnitHeader
{
    NalUnitType unitType = NalUnitType::UnspecStart;
    int layerId = 0;
    int tid = 0; // TemporalId + 1

    static const int kForbiddenFieldLength = 1;
    static const int kUnitTypeFieldLength = 6;
    static const int kLayerIdFieldLength = 6;
    static const int kTidFieldLength = 3;

    static const int kTotalLength = kForbiddenFieldLength
        + kUnitTypeFieldLength
        + kLayerIdFieldLength
        + kTidFieldLength;

    bool decodeFromPayload(const uint8_t* const payload, int payloadLength);
};

bool isValidUnitType(NalUnitType unitType);
bool isRandomAccessPoint(NalUnitType unitType);
bool isLeadingPicture(NalUnitType unitType);
bool isTrailingPicture(NalUnitType unitType);

} // namespace hevc
} // namespace media_utils
} // namespace nx