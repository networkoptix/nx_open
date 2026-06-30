// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "h2645_prepend_parameter_sets.h"

#include <nx/codec/h264/common.h>
#include <nx/codec/h265/hevc_common.h>
#include <nx/codec/nal_extradata.h>
#include <nx/codec/nal_units.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>

namespace {

QnCompressedVideoDataPtr cloneWithPrependedHeader(
    const QnConstCompressedVideoDataPtr& videoData,
    const std::vector<uint8_t>& header)
{
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    result->QnCompressedVideoData::assign(videoData.get());
    result->m_data.resize(header.size() + videoData->dataSize());

    uint8_t* dst = reinterpret_cast<uint8_t*>(result->m_data.data());
    memcpy(dst, header.data(), header.size());
    memcpy(dst + header.size(), videoData->data(), videoData->dataSize());

    return result;
}

bool h264HasKeyframeWithoutParamSets(const std::vector<nx::media::nal::NalUnitInfo>& units)
{
    bool hasIdr = false;
    bool hasParamSets = false;
    for (const auto& unit: units)
    {
        const int type = nx::media::h264::nalType(*unit.data);
        if (type == nx::media::h264::nuSliceIDR)
            hasIdr = true;
        if (type == nx::media::h264::nuSPS || type == nx::media::h264::nuPPS)
            hasParamSets = true;
    }
    return hasIdr && !hasParamSets;
}

bool h265HasKeyframeWithoutParamSets(const std::vector<nx::media::nal::NalUnitInfo>& units)
{
    bool hasIrap = false;
    bool hasParamSets = false;
    for (const auto& unit: units)
    {
        const auto type = nx::media::h265::nalType(*unit.data);
        if (nx::media::h265::isRandomAccessPoint(type))
            hasIrap = true;
        if (nx::media::h265::isParameterSet(type))
            hasParamSets = true;
    }
    return hasIrap && !hasParamSets;
}

} // namespace

QnConstAbstractDataPacketPtr H2645PrependParameterSets::processData(
    const QnConstAbstractDataPacketPtr& data)
{
    const auto videoData = std::dynamic_pointer_cast<const QnCompressedVideoData>(data);
    if (!videoData || !videoData->context)
        return data;

    if (!(videoData->flags & QnAbstractMediaData::MediaFlags_AVKey))
        return data;

    const bool isH264 = videoData->compressionType == AV_CODEC_ID_H264;
    const bool isH265 = videoData->compressionType == AV_CODEC_ID_H265;
    if (!isH264 && !isH265)
        return data;

    const uint8_t* extraData = videoData->context->getExtradata();
    const int extraDataSize = videoData->context->getExtradataSize();
    if (!extraData || extraDataSize <= 0)
        return data;

    const bool isAnnexB = nx::media::nal::isStartCode(extraData, extraDataSize);
    const auto units = isAnnexB
        ? nx::media::nal::findNalUnitsAnnexB(
            (const uint8_t*)videoData->data(), (int32_t)videoData->dataSize())
        : nx::media::nal::findNalUnitsMp4(
            (const uint8_t*)videoData->data(), (int32_t)videoData->dataSize());

    const bool needsPrepend = isH264
        ? h264HasKeyframeWithoutParamSets(units)
        : h265HasKeyframeWithoutParamSets(units);

    if (!needsPrepend)
        return data;

    std::vector<uint8_t> header;
    if (isAnnexB)
    {
        header.assign(extraData, extraData + extraDataSize);
    }
    else
    {
        auto headerAnnexB = isH264
            ? nx::media::nal::readH264SeqHeaderFromExtraData(extraData, extraDataSize)
            : nx::media::nal::readH265SeqHeaderFromExtraData(extraData, extraDataSize);

        if (headerAnnexB.empty())
        {
            NX_WARNING(this, "Failed to read parameter sets from extradata");
            return data;
        }
        header = nx::media::nal::convertStartCodesToSizes(
            headerAnnexB.data(), headerAnnexB.size());
    }

    return cloneWithPrependedHeader(videoData, header);
}
