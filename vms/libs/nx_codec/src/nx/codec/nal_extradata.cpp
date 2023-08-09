// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nal_extradata.h"

#include <nx/codec/hevc/hevc_decoder_configuration_record.h>
#include <nx/codec/nal_units.h>
#include <nx/utils/log/log.h>

namespace nx::media::nal {

namespace {

int getNalSize(const uint8_t* ptr)
{
    return (ptr[0] << 8) | ptr[1];
}

void appendNalUnit(std::vector<uint8_t>& result, const uint8_t* data, int size)
{
    result.insert(result.end(), nx::media::nal::kStartCode.begin(), nx::media::nal::kStartCode.end());
    result.insert(result.end(), data, data + size);
}

} // namespace

// TODO: Code duplication with "h264_utils.cpp".
std::vector<uint8_t> readH264SeqHeaderFromExtraData(const uint8_t* extraData, int extraDataSize)
{
    std::vector<uint8_t> result;
    if (!extraData || extraDataSize < 8 || extraData[0] != 1)
        return result;

    const uint8_t* p = extraData;
    //sps & pps is in the extradata, parsing it...
    //following code has been taken from libavcodec/h264.c

    int lengthSizeMinusOne = (extraData[4] & 0x03);
    if (lengthSizeMinusOne != 3)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported NAL length size: %1, TODO impl this",
            lengthSizeMinusOne + 1);
        return result;
    }

    // Decode sps from avcC
    int cnt = *(p + 5) & 0x1f; // Number of sps
    p += 6;

    for (int i = 0; i < cnt; i++)
    {
        const int nalsize = getNalSize(p);
        p += 2; //skipping nalusize
        if (nalsize > extraDataSize - (p - extraData))
            break;
        appendNalUnit(result, p, nalsize);
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for (int i = 0; i < cnt; ++i)
    {
        const int nalsize = getNalSize(p);
        p += 2;
        if (nalsize > extraDataSize - (p - extraData))
            break;

        appendNalUnit(result, p, nalsize);
        p += nalsize;
    }
    return result;
}

std::vector<uint8_t> readH265SeqHeaderFromExtraData(const uint8_t* extraData, int extraDataSize)
{
    std::vector<uint8_t> result;
    if (!extraData)
        return result;

    nx::media::hevc::HEVCDecoderConfigurationRecord hvcc;
    if (!hvcc.read(extraData, extraDataSize))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to parse hvcc");
        return result;
    }
    if (hvcc.lengthSizeMinusOne != 3)
    {
        NX_WARNING(NX_SCOPE_TAG, "Unsupported NAL length size: %1, TODO impl this",
            hvcc.lengthSizeMinusOne + 1);
        return result;
    }
    for (auto& vps: hvcc.vps)
        appendNalUnit(result, vps.data(), vps.size());
    for (auto& sps: hvcc.sps)
        appendNalUnit(result, sps.data(), sps.size());
    for (auto& pps: hvcc.pps)
        appendNalUnit(result, pps.data(), pps.size());
    return result;
}

} // namespace nx::media::nal
