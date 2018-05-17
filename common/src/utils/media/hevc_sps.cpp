#include "hevc_sps.h"
#include "hevc_common.h"

#include <memory>

#include <utils/media/bitStream.h>
#include <utils/media/nalUnits.h>
#include <nx/streaming/video_data_packet.h>
#include "h264_utils.h"

namespace nx {
namespace media_utils {
namespace hevc {

namespace {

static const int kReservedNalSpace = 16; //< Where this number comes from?

} // namespace

bool Sps::decodeFromVideoFrame(const QnConstCompressedVideoDataPtr& videoData)
{
    using namespace nx::media_utils;

    // H.265 nal units have same format (unit delimiter) as H.264 nal units
    std::vector<std::pair<const quint8*, size_t>> nalUnits =
        nx::media_utils::avc::decodeNalUnits(videoData);

    for (const std::pair<const quint8*, size_t>& nalu : nalUnits)
    {
        hevc::NalUnitHeader packetHeader;
        if (!packetHeader.decode(nalu.first, nalu.second))
            return false;

        switch (packetHeader.unitType)
        {
            case hevc::NalUnitType::spsNut:
                return decode(nalu.first, nalu.second);
            default:
                break;
        }
    }
    return false;
}

bool Sps::decode(const uint8_t* payload, int payloadLength)
{
    payload += NalUnitHeader::kTotalLength;
    payloadLength -= NalUnitHeader::kTotalLength;

    std::vector<uint8_t> decodedNalBuffer(payloadLength + kReservedNalSpace);
    auto rawDecodedNalBuffer = decodedNalBuffer.data();

    NALUnit::decodeNAL(
        payload,
        payload + payloadLength,
        rawDecodedNalBuffer,
        payloadLength + kReservedNalSpace);

    BitStreamReader reader;

    try
    {
        reader.setBuffer(rawDecodedNalBuffer, rawDecodedNalBuffer + payloadLength);
        spsVideoParamterSetId = reader.getBits(4);
        spsMaxSubLayersMinus1 = reader.getBits(3);
        spsTemporalIdNestingFlag = reader.getBit();
        bool result = decodeProfileTierLevel(
            reader,
            /*profileFlagPresent*/ true,
            spsMaxSubLayersMinus1,
            &profileTierLevel);

        if (!result)
            return false;

        spsSeqParameterSetId = NALUnit::extractUEGolombCode(reader);

        chromaFormatIdc = NALUnit::extractUEGolombCode(reader);

        if (chromaFormatIdc == 3)
            separateColourPlaneFlag = reader.getBit();

        picWidthInLumaSamples = NALUnit::extractUEGolombCode(reader);
        picHeightInLumaSamples = NALUnit::extractUEGolombCode(reader);

        // Stop decoding since we do not need further fields.
        // TODO: #dmishin implement full decoding.
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool Sps::decodeProfileTierLevel(
    BitStreamReader& reader,
    bool profilePresesentFlag,
    int maxSublayersMinus1,
    ProfileTierLevel* outProfileTierLevel)
{
    if (profilePresesentFlag)
        decodeLayer(reader, &(outProfileTierLevel->general));
    else
        outProfileTierLevel->general.levelIdc = reader.getBits(8);

    for (auto i = 0; i < maxSublayersMinus1; ++i)
    {
        ProfileTierLevel::SubLayerPresenceFlags flags;

        flags.subLayerProfilePresentFlag = reader.getBit();
        flags.subLayerLevelPresentFlag = reader.getBit();

        outProfileTierLevel->subLayerPresenceFlags.push_back(flags);
    }

    if (maxSublayersMinus1 > 0)
    {
        for (auto i = maxSublayersMinus1; i < 8; ++i)
            reader.skipBits(2); // < reserved_zero_2bits
    }

    for (auto i = 0; i < maxSublayersMinus1; ++i)
    {
        if (outProfileTierLevel->subLayerPresenceFlags[i].subLayerProfilePresentFlag)
        {
            ProfileTierLevel::Layer subLayer;
            subLayer.setIsSubLayer(true);
            decodeLayer(reader, &subLayer);
            outProfileTierLevel->subLayers.push_back(subLayer);
        }
    }

    return true;
}

bool Sps::decodeLayer(BitStreamReader& reader, ProfileTierLevel::Layer* outLayer)
{
    outLayer->profileSpace = reader.getBits(2);
    outLayer->tierFlag = reader.getBit();
    outLayer->profileIdc = reader.getBits(5);
    for (auto i = 0; i < outLayer->profileCompatibilityFlags.size(); ++i)
        outLayer->profileCompatibilityFlags[i] = reader.getBit();

    outLayer->progressiveSourceFlag = reader.getBit();
    outLayer->interlacedSourceFlag = reader.getBit();
    outLayer->nonPackedSourceFlag = reader.getBit();
    outLayer->frameOnlyConstraintFlag = reader.getBit();

    bool condition = outLayer->profileIdc == 4
        || outLayer->profileIdc == 5
        || outLayer->profileIdc == 6
        || outLayer->profileIdc == 7
        || outLayer->profileIdc == 8
        || outLayer->profileIdc == 9
        || outLayer->profileIdc == 10
        || outLayer->profileCompatibilityFlags[4]
        || outLayer->profileCompatibilityFlags[5]
        || outLayer->profileCompatibilityFlags[6]
        || outLayer->profileCompatibilityFlags[7]
        || outLayer->profileCompatibilityFlags[8]
        || outLayer->profileCompatibilityFlags[9]
        || outLayer->profileCompatibilityFlags[10];

    if (condition)
    {
        outLayer->max12BitConstraintFlag = reader.getBit();
        outLayer->max10BitConstraintFlag = reader.getBit();
        outLayer->max8BitConstraintFlag = reader.getBit();
        outLayer->max422ChromaConstraintFlag = reader.getBit();
        outLayer->max420ChromaConstraintFlag = reader.getBit();
        outLayer->maxMonochromeConstraintFlag = reader.getBit();
        outLayer->intraConstraintFlag = reader.getBit();
        outLayer->onePictureOnlyConstraintFlag = reader.getBit();
        outLayer->lowerBitRateConstraintFlag = reader.getBit();

        condition = outLayer->profileIdc == 5
            || outLayer->profileCompatibilityFlags[5];

        if (!outLayer->isSubLayer())
        {
            condition |= outLayer->profileIdc == 9
                || outLayer->profileIdc == 10
                || outLayer->profileCompatibilityFlags[9]
                || outLayer->profileCompatibilityFlags[10];
        }

        if (condition)
        {
            outLayer->max14BitConstraintFlag = reader.getBit();
            reader.skipBits(32);
            reader.skipBit(); //< reserved_zero_33_bits
        }
        else
        {
            reader.skipBits(32);
            reader.skipBits(2); //< reserved_zero_34_bits
        }
    }
    else
    {
        reader.skipBits(32);
        reader.skipBits(11); //< reserved_zero_43_bits
    }

    condition = (outLayer->profileIdc >= 1 && outLayer->profileIdc <= 5)
        || outLayer->profileIdc == 9
        || outLayer->profileCompatibilityFlags[1]
        || outLayer->profileCompatibilityFlags[2]
        || outLayer->profileCompatibilityFlags[3]
        || outLayer->profileCompatibilityFlags[4]
        || outLayer->profileCompatibilityFlags[5]
        || outLayer->profileCompatibilityFlags[9];

    if (condition)
        outLayer->inbldFlag = reader.getBit();

    outLayer->levelIdc = reader.getBits(8);

    return true;
}

} // namespace hevc
} // namespace media_utils
} // namespace nx
