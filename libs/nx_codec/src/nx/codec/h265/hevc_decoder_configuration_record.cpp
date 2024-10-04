// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hevc_decoder_configuration_record.h"

#include <nx/codec/h265/hevc_common.h>
#include <nx/utils/bit_stream.h>

namespace {

int sizeOfNaluVector(const std::vector<std::vector<uint8_t>>& nalUnits)
{
    int result = nalUnits.size() * 3; // type filed + nal unit count field
    for (auto& unit: nalUnits)
        result += unit.size() + 2/*nal unit size field*/;
    return result;
}

void writeNaluVector(
    nx::utils::BitStreamWriter& writer, const std::vector<std::vector<uint8_t>>& nalUnits, int type)
{
    writer.putBits(2, 0);
    writer.putBits(6, type);
    writer.putBits(16, nalUnits.size());
    for (const auto& unit: nalUnits)
    {
        writer.putBits(16, unit.size());
        writer.putBytes(unit.data(), unit.size());
    }
}

}

namespace nx::media::h265 {

bool HEVCDecoderConfigurationRecord::read(const uint8_t* data, int size)
{
    constexpr int kMinSizeBytes = 23;
    if (size < kMinSizeBytes)
        return false;
    try
    {
        nx::utils::BitStreamReader reader(data, size);
        configurationVersion = reader.getBits(8);
        general_profile_space = reader.getBits(2);
        general_tier_flag = reader.getBits(1);
        general_profile_idc = reader.getBits(5);
        general_profile_compatibility_flags = reader.getBits(32);

        //TODO support 64 bit fields general_constraint_indicator_flags = reader.getBits(48);
        general_constraint_indicator_flags = reader.getBits(32);
        general_constraint_indicator_flags |= (int64_t(reader.getBits(16)) << 32);

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
                std::vector<uint8_t> nalData(nalUnitLength);
                reader.readData(nalData.data(), nalUnitLength);
                if (NAL_unit_type == NalUnitType::spsNut)
                    sps.push_back(nalData);
                else if (NAL_unit_type == NalUnitType::ppsNut)
                    pps.push_back(nalData);
                else if (NAL_unit_type == NalUnitType::vpsNut)
                    vps.push_back(nalData);
            }
        }
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}

bool HEVCDecoderConfigurationRecord::write(uint8_t* data, int size) const
{
    constexpr int kMinSizeBytes = 23;
    if (size < kMinSizeBytes)
        return false;
    try
    {
        nx::utils::BitStreamWriter writer(data, size);
        writer.putBits(8, configurationVersion);
        writer.putBits(2, general_profile_space);
        writer.putBits(1, general_tier_flag);
        writer.putBits(5, general_profile_idc);
        writer.putBits(32, general_profile_compatibility_flags);
        writer.putBits(32, general_constraint_indicator_flags);
        writer.putBits(16, general_constraint_indicator_flags >> 32);
        writer.putBits(8, general_level_idc);
        writer.putBits(4, 0xff);
        writer.putBits(12, min_spatial_segmentation_idc);
        writer.putBits(6, 0xff);
        writer.putBits(2, parallelismType);
        writer.putBits(6, 0xff);
        writer.putBits(2, chromaFormat);
        writer.putBits(5, 0xff);
        writer.putBits(3, bitDepthLumaMinus8);
        writer.putBits(5, 0xff);
        writer.putBits(3, bitDepthChromaMinus8);
        writer.putBits(16, bitDepthChromaMinus8);
        writer.putBits(2, constantFrameRate);
        writer.putBits(3, numTemporalLayers);
        writer.putBits(1, temporalIdNested);
        writer.putBits(2, lengthSizeMinusOne);
        writer.putBits(8, sps.size() + pps.size() + vps.size());
        writeNaluVector(writer, vps, (int)NalUnitType::vpsNut);
        writeNaluVector(writer, sps, (int)NalUnitType::spsNut);
        writeNaluVector(writer, pps, (int)NalUnitType::ppsNut);
        writer.flushBits();
    }
    catch (const nx::utils::BitStreamException&)
    {
        return false;
    }
    return true;
}


int HEVCDecoderConfigurationRecord::size() const
{
    int result =
        sizeof(configurationVersion) + // 1
        1 + /* general_profile_space(2) + general_tier_flag(1) + general_profile_idc(5) */
        sizeof(general_profile_compatibility_flags) + // 4
        6 + /* general_constraint_indicator_flags(48) */
        sizeof(general_level_idc) + // 1
        sizeof(min_spatial_segmentation_idc) + // 2
        sizeof(parallelismType) + // 1
        sizeof(chromaFormat) + // 1
        sizeof(bitDepthLumaMinus8) + // 1
        sizeof(bitDepthChromaMinus8) + // 1
        sizeof(avgFrameRate) + // 2
        1 + /* constantFrameRate(2) + numTemporalLayers(3) + temporalIdNested(1) + lengthSizeMinusOne(2) */
        1; /* nalu arrays count field */
    result += sizeOfNaluVector(sps);
    result += sizeOfNaluVector(pps);
    result += sizeOfNaluVector(vps);
    return result;
}

} // namespace nx::media::h265
