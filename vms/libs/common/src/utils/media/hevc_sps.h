#pragma once

#include <array>
#include <vector>

#include <utils/media/bitStream.h>

class QnCompressedVideoData;

namespace nx {
namespace media_utils {
namespace hevc {

// TODO: #dmishin refactor to embed HEVC NALs to existing H264 hierarchy.

using DecodedUev = uint32_t;

struct ProfileTierLevel
{
    struct SubLayerPresenceFlags
    {
        bool subLayerProfilePresentFlag = false;
        bool subLayerLevelPresentFlag = false;
    };

    struct Layer
    {
        uint8_t profileSpace = 0;
        bool tierFlag = false;
        uint8_t profileIdc = 0;
        std::array<bool, 32> profileCompatibilityFlags;
        bool progressiveSourceFlag = false;
        bool interlacedSourceFlag = false;
        bool nonPackedSourceFlag = false;
        bool frameOnlyConstraintFlag = false;
        bool max12BitConstraintFlag = false;
        bool max10BitConstraintFlag = false;
        bool max8BitConstraintFlag = false;
        bool max422ChromaConstraintFlag = false;
        bool max420ChromaConstraintFlag = false;
        bool maxMonochromeConstraintFlag = false;
        bool intraConstraintFlag = false;
        bool onePictureOnlyConstraintFlag = false;
        bool lowerBitRateConstraintFlag = false;
        bool max14BitConstraintFlag = false;
        bool inbldFlag = false;
        uint8_t levelIdc = 0;

    public:
        bool isSubLayer() { return nxIsSubLayer; } const
        void setIsSubLayer(bool isSubLayer) { nxIsSubLayer = isSubLayer; }

    private:
        // Internal Nx field, shouldn't be serialized.
        bool nxIsSubLayer = false;
    };

    Layer general;
    std::vector<SubLayerPresenceFlags> subLayerPresenceFlags;
    std::vector<Layer> subLayers;
};

struct VuiParameters
{
    // TODO: #dmishin implement
};

struct ShortTermRefPicSet
{
    // TODO: #dmishin implement
};

struct LongTermRefPicSet
{
    // TODO: #dmishin implement
};

struct ScalingListData
{
    // TODO: #dmishin implement
};

struct Sps
{
public:
    // Decode a single SPS NAL unit. Now it decodes SPS only till conformance_window_flag field
    bool decode(const uint8_t* payload, int payloadLength);

    // Decode from video frame that can contains many NAL units.
    bool decodeFromVideoFrame(const std::shared_ptr<const QnCompressedVideoData>& videoData);
private:
    bool decodeProfileTierLevel(
        BitStreamReader& reader,
        bool profilePresentFlag,
        int maxSublayersMinus1,
        ProfileTierLevel* outProfileTierLevel);

    bool decodeLayer(
        BitStreamReader& reader,
        ProfileTierLevel::Layer* outLayer);

public:
    struct OrderingInfo
    {
        DecodedUev spsMaxDecPicBufferingMinus1 = 0;
        DecodedUev spsMaxNumReorderPics = 0;
        DecodedUev spsMaxLatencyIncreasePlus1 = 0;
    };

    struct LongTermRefPicSps
    {
        DecodedUev ltRefPicPocLsbSps = 0;
        bool usedByCurrPicLtSpsFlag = false;
    };

    struct SpsRangeExtension
    {
        bool transformSkipRotationEnabledFlag = false;
        bool transformSkipContextEnabledFlag = false;
        bool implicitRdpcmEnabledFlag = false;
        bool explicitRdpcmEnabledFlag = false;
        bool extendedPrecisionProcessingFlag = false;
        bool intraSmoothingDisabledFlag = false;
        bool highPrecisionOffsetsEnabledFlag = false;
        bool persistentRiceAdaptationEnabledFlag = false;
        bool cabacBypassAlignmentEnabledFlag = false;
    };

    uint8_t spsVideoParamterSetId = 0;
    uint8_t spsMaxSubLayersMinus1 = 0;
    bool spsTemporalIdNestingFlag = false;
    ProfileTierLevel profileTierLevel;
    DecodedUev spsSeqParameterSetId = 0;
    DecodedUev chromaFormatIdc = 0;
    bool separateColourPlaneFlag = false;
    DecodedUev picWidthInLumaSamples = 0;
    DecodedUev picHeightInLumaSamples = 0;
    bool conformanceWindowFlag = false;
    DecodedUev confWinLeftOffset = 0;
    DecodedUev confWinRightOffset = 0;
    DecodedUev confWinTopOffset = 0;
    DecodedUev confWinBottomOffset = 0;
    DecodedUev bitDepthLumaMinus8 = 0;
    DecodedUev bitDepthChromaMinus8 = 0;
    DecodedUev log2MaxPicOrderCntLsbMinus4 = 0;
    bool spsSubLayerOrderingInfoPresentFlag = false;
    std::vector<OrderingInfo> orderingInfo;
    DecodedUev log2MinLumaCodingBlockSizeMinus3 = 0;
    DecodedUev log2DiffMaxMinLumaCodingBlockSize = 0;
    DecodedUev log2MinLumaTransformBlockSizeMinus2 = 0;
    DecodedUev log2DiffMaxMinLumaTransformBlockSize = 0;
    DecodedUev maxTransformHierarchyDepthInter = 0;
    DecodedUev maxTransformHierarchyDepthIntra = 0;
    bool scalingListEnabledFlag = false;
    bool spsScalingListDataPresentFlag = false;
    ScalingListData scalingListdata;
    bool ampEnabledFlag = false;
    bool sampleAdaptiveOffsetEnabledFlag = false;
    bool pcmEnabledFlag = false;
    uint8_t pcmSampleBitDepthLumaMinus1 = 0;
    uint8_t pcmSampleBitDepthChromaMinus1 = 0;
    DecodedUev log2MinPcmLumaCodingBlockSizeMinus3 = 0;
    DecodedUev log2DiffMaxMinPcmLumaCodingBlockSize = 0;
    bool pcmLoopFilterDisabledFlag = false;
    DecodedUev numShortTermRefPicSets = 0;
    std::vector<ShortTermRefPicSet> shortTermRefPicSet;
    bool longTermRefPicsPresentFlag = false;
    DecodedUev numLongTermRefPicsSps = 0;
    std::vector<LongTermRefPicSps> longTermRefPicsSps;
    bool spsTemporalMvpEnabledFlag = false;
    bool strongIntraSmoothingEnabledFlag = false;
    bool vuiParametersPresentFlag = false;
    VuiParameters vuiParameters;
    bool spsExtensionPresentFlag = false;
    bool spsRangeExtensionFlag = false;
    bool spsMultilayerExtensionFlag = false;
    bool sps3dExtensionFlag = false;
    bool spsSccExtensionFlag = false;
    uint8_t spsExtension4Bits = 0;
    // spsRangeExtension
    // spsMultilayerExtension
    // sps3dExtension
    // spsSccExtension
};

} // namespace hevc
} // namespace media_utils
} // namespace nx
