// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "h264_utils.h"

#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace nx::media::h264 {

namespace {

int getNalSize(const uint8_t* ptr)
{
    return (ptr[0] << 8) | ptr[1];
}

} // namespace

// TODO: Code duplication with "h264_mp4_to_annexb.cpp".
/**
 * @param data data->context should not be null.
 */
std::vector<nal::NalUnitInfo> readH264NALUsFromExtraData(
    const uint8_t* extradata, int extradataSize)
{
    std::vector<nal::NalUnitInfo> result;
    const uint8_t* p = extradata;

    //sps & pps is in the extradata, parsing it...
    //following code has been taken from libavcodec/h264.c

    // prefix is unit len
    //const int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;
    /* sps and pps in the avcC always have length coded with 2 bytes,
    * so put a fake nal_length_size = 2 while parsing them */
    //int nal_length_size = 2;

    // Decode sps from avcC
    int cnt = *(p + 5) & 0x1f; // Number of sps
    p += 6;

    for (int i = 0; i < cnt; i++)
    {
        const int nalsize = getNalSize(p);
        p += 2; //skipping nalusize
        if (nalsize > extradataSize - (p - extradata))
            break;
        result.emplace_back(nal::NalUnitInfo{p, nalsize});
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for (int i = 0; i < cnt; ++i)
    {
        const int nalsize = getNalSize(p);
        p += 2;
        if (nalsize > extradataSize - (p - extradata))
            break;
        result.emplace_back(nal::NalUnitInfo{p, nalsize});
        p += nalsize;
    }
    return result;
}

bool extractSps(const QnCompressedVideoData* videoData, SPSUnit& sps)
{
    auto nalUnits = decodeNalUnits(videoData);

    for (const auto& nalu: nalUnits)
    {
        if (NALUnit::decodeType(*nalu.data) == nuSPS)
        {
            sps.decodeBuffer(nalu.data, nalu.data + nalu.size);
            return sps.deserialize() == 0;
        }
    }
    return false;
}

bool isH264SeqHeaderInExtraData(const QnCompressedVideoData* data)
{
    return data->context &&
        data->context->getExtradataSize() >= 7 &&
        data->context->getExtradata()[0] == 1;
}

std::vector<nal::NalUnitInfo> decodeNalUnits(
    const QnCompressedVideoData* data)
{
    if (isH264SeqHeaderInExtraData(data))
        return readH264NALUsFromExtraData(data->context->getExtradata(), data->context->getExtradataSize());
    else
        return nal::findNalUnitsAnnexB((const uint8_t*)data->data(), data->dataSize());
}

std::vector<uint8_t> buildExtraDataAnnexB(const uint8_t* data, int32_t size)
{
    std::vector<uint8_t> extraData;
    std::vector<uint8_t> startcode = {0x0, 0x0, 0x0, 0x1};
    auto nalUnits = nx::media::nal::findNalUnitsAnnexB(data, size);
    for (const auto& nalu: nalUnits)
    {
        const auto nalType = NALUnit::decodeType(*nalu.data);
        if (nalType == nuSPS || nalType == nuPPS)
        {
            extraData.insert(extraData.end(), startcode.begin(), startcode.end());
            extraData.insert(extraData.end(), nalu.data, nalu.data + nalu.size);
        }
    }
    return extraData;
}

std::vector<uint8_t> buildExtraDataMp4FromAnnexB(const uint8_t* data, int32_t size)
{
    return buildExtraDataMp4(nal::findNalUnitsAnnexB(data, size));
}

std::vector<uint8_t> buildExtraDataMp4(const std::vector<nal::NalUnitInfo>& nalUnits)
{
    std::vector<std::vector<uint8_t>> spsVector;
    std::vector<std::vector<uint8_t>> ppsVector;
    for (const auto& nalu: nalUnits)
    {
        const auto nalType = NALUnit::decodeType(*nalu.data);
        if (nalType == nuSPS)
            spsVector.emplace_back(nalu.data, nalu.data + nalu.size);
        else if (nalType == nuPPS)
            ppsVector.emplace_back(nalu.data, nalu.data + nalu.size);
    }
    if (spsVector.empty() || ppsVector.empty())
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to write h264 extra data, no sps/pps found");
        return std::vector<uint8_t>();
    }

    // Get extra data size.
    int extradataSize = 5; // Version, profile, profile compat, level, nal size length.
    extradataSize += 1; // Sps count.
    for(const auto& sps: spsVector)
    {
        extradataSize += sizeof(uint16_t); // Sps size.
        extradataSize += sps.size(); // Sps data.
    }
    extradataSize += 1; // pps count
    for(const auto& pps: ppsVector)
    {
        extradataSize += sizeof(uint16_t); // Pps size.
        extradataSize += pps.size(); // Pps data.
    }

    auto sps = spsVector[0];
    auto pps = ppsVector[0];
    std::vector<uint8_t> extradata(extradataSize);
    nx::utils::BitStreamWriter bitstream(extradata.data(), extradata.data() + extradataSize);
    try
    {
        // See ISO_IEC_14496-15 AVCDecoderConfigurationRecord.
        bitstream.putBits(8, 1); // Version.
        bitstream.putBits(8, sps[1]); // Profile.
        bitstream.putBits(8, sps[2]); // Profile compat.
        bitstream.putBits(8, sps[3]); // Profile level.
        bitstream.putBits(8, 0xff); // 6 bits reserved(111111) + 2 bits nal size length - 1: 3(11).

        uint8_t numSps = uint8_t(spsVector.size()) | 0xe0;
        bitstream.putBits(8, numSps); // 3 bits reserved (111) + 5 bits number of sps.

        for(const auto& spsData: spsVector)
        {
            bitstream.putBits(16, spsData.size());
            bitstream.putBytes(spsData.data(), spsData.size());
        }
        bitstream.putBits(8, ppsVector.size()); // Number of pps.
        for(const auto& ppsData: ppsVector)
        {
            bitstream.putBits(16, ppsData.size());
            bitstream.putBytes(ppsData.data(), ppsData.size());
        }
        bitstream.flushBits();
    }
    catch(const nx::utils::BitStreamException&)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to write extra data");
        return std::vector<uint8_t>();
    }
    return extradata;
}

} // namespace nx::media::h264
