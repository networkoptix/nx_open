#include "hevc_decoder_configuration_record.h"

#include "bitStream.h"
#include "hevc_common.h"

namespace nx::media_utils::hevc {

bool HEVCDecoderConfigurationRecord::parse(const uint8_t* data, int size)
{
    constexpr int kMinSizeBytes = 23;
    if (size < kMinSizeBytes)
        return false;
    try
    {
        BitStreamReader reader(data, size);
        configurationVersion = reader.getBits(8);
        general_profile_space = reader.getBits(2);
        general_tier_flag = reader.getBits(1);
        general_profile_idc = reader.getBits(5);
        general_profile_compatibility_flags = reader.getBits(32);

        //TODO support 64 bit fields general_constraint_indicator_flags = reader.getBits(48);
        reader.skipBits(32);
        reader.skipBits(16);

        general_level_idc = reader.getBits(8);
        reader.skipBits(4);
        min_spatial_segmentation_idc = reader.getBits(12);
        reader.skipBits(6);
        parallelismType = reader.getBits(2);
        reader.skipBits(6);
        chromaFormat = reader.getBits(2);
        reader.skipBits(5);
        bitDepthLumaMinus8 = reader.getBits(3);
        reader.skipBits(5);
        bitDepthChromaMinus8 = reader.getBits(3);
        avgFrameRate = reader.getBits(16);
        constantFrameRate = reader.getBits(2);
        numTemporalLayers = reader.getBits(3);
        temporalIdNested = reader.getBits(1);
        lengthSizeMinusOne = reader.getBits(2);
        int numOfArrays = reader.getBits(8);
        for (int i = 0; i < numOfArrays; ++i)
        {
            reader.skipBits(2);
            NalUnitType NAL_unit_type = (NalUnitType)reader.getBits(6);
            int numNalus = reader.getBits(16);
            for (int j = 0; j< numNalus; ++j)
            {
                int nalUnitLength = reader.getBits(16);
                std::vector<uint8_t> data(nalUnitLength);
                reader.readData(data.data(), nalUnitLength);
                if (NAL_unit_type == NalUnitType::spsNut)
                    sps.push_back(data);
                else if (NAL_unit_type == NalUnitType::ppsNut)
                    pps.push_back(data);
                else if (NAL_unit_type == NalUnitType::vpsNut)
                    vps.push_back(data);
            }
        }
    }
    catch (const BitStreamException&)
    {
        return false;
    }
    return true;
}

} // namespace nx::media_utils::hevc
