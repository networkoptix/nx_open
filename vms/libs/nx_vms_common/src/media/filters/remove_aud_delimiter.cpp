// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

////////////////////////////////////////////////////////////
// 20 feb 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "remove_aud_delimiter.h"

#include <array>

#include <nx/codec/nal_extradata.h>
#include <nx/codec/nal_units.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>

namespace {

inline int h264NalType(const uint8_t* nal)
{
    return int(nal[0] & 0x1F);
}

inline int h265NalType(const uint8_t* nal)
{
    return int((nal[0] >> 1) & 0x3F);
}

uint32_t readBe32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

} // namespace

QnConstAbstractDataPacketPtr H2645RemoveAudDelimiter::processData(const QnConstAbstractDataPacketPtr& data)
{
    const QnConstCompressedVideoDataPtr videoData =
        std::dynamic_pointer_cast<const QnCompressedVideoData>(data);

    if (!videoData)
        return data;

    auto result = processVideoData(videoData);
    if (!result)
        return data;

    return result;
}

QnCompressedVideoDataPtr H2645RemoveAudDelimiter::removeAuxFromAnexBFormat(
    const QnConstCompressedVideoDataPtr& videoData)
{
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData(videoData->dataSize()));
    result->QnCompressedVideoData::assign(videoData.get());

    const auto units = nx::media::nal::findNalUnitsAnnexB(
        (const uint8_t*)videoData->data(), videoData->dataSize());

    bool hasAud = false;
    for (const auto& unit: units)
    {
        bool isAudNalForCodec = (videoData->compressionType == AV_CODEC_ID_H264)
            ? (h264NalType(unit.data) == 9) // AVC AUD
            : (h265NalType(unit.data) == 35); // HEVC AUD
        if (isAudNalForCodec)
        {
            hasAud = true;
            break;
        }
    }
    if (!hasAud)
        return nullptr; // Keep unchanged

    uint8_t* dst = (uint8_t*)result->data();
    for (const auto& unit: units)
    {
        bool isAudNalForCodec = (videoData->compressionType == AV_CODEC_ID_H264)
            ? (h264NalType(unit.data) == 9) // AVC AUD
            : (h265NalType(unit.data) == 35); // HEVC AUD
        if (!isAudNalForCodec)
        {
            memcpy(dst, "\x0\x0\x0\x1", 4);
            dst += 4;
            memcpy(dst, unit.data, unit.size);
            dst += unit.size;
        }
    }
    result->m_data.resize(dst - (uint8_t*) result->data());
    return result;
}

QnCompressedVideoDataPtr H2645RemoveAudDelimiter::removeAuxFromMp4Format(
    const QnConstCompressedVideoDataPtr& videoData)
{
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData(videoData->dataSize()));
    result->QnCompressedVideoData::assign(videoData.get());

    const auto units = nx::media::nal::findNalUnitsMp4(
        (const uint8_t*)videoData->data(), videoData->dataSize());

    bool hasAud = false;
    for (const auto& unit : units)
    {
        bool isAudNalForCodec = (videoData->compressionType == AV_CODEC_ID_H264)
            ? (h264NalType(unit.data) == 9) // AVC AUD
            : (h265NalType(unit.data) == 35); // HEVC AUD
        if (isAudNalForCodec)
        {
            hasAud = true;
            break;
        }
    }
    if (!hasAud)
        return nullptr; // Keep unchanged

    uint8_t* dst = (uint8_t*)result->data();
    for (const auto& unit : units)
    {
        bool isAudNalForCodec = (videoData->compressionType == AV_CODEC_ID_H264)
            ? (h264NalType(unit.data) == 9) // AVC AUD
            : (h265NalType(unit.data) == 35); // HEVC AUD
        if (!isAudNalForCodec)
        {
            memcpy(dst, unit.data - 4, unit.size + 4);
            dst += unit.size + 4;
        }
    }
    result->m_data.resize(dst - (uint8_t*)result->data());
    return result;
}

QnCompressedVideoDataPtr H2645RemoveAudDelimiter::processVideoData(
    const QnConstCompressedVideoDataPtr& videoData)
{
    auto codecId = videoData->compressionType;
    if (codecId != AV_CODEC_ID_H264 && codecId != AV_CODEC_ID_H265)
        return nullptr;

    if (!videoData->context)
    {
        if (nx::media::nal::isStartCode(videoData->data(), videoData->dataSize()))
            return removeAuxFromAnexBFormat(videoData);
        return nullptr;
    }

    std::vector<uint8_t> header;
    const uint8_t* extraData = videoData->context->getExtradata();
    int extraDataSize = videoData->context->getExtradataSize();

    if (extraData && extraDataSize)
    {
        if (nx::media::nal::isStartCode(extraData, extraDataSize))
            return removeAuxFromAnexBFormat(videoData);
        return removeAuxFromMp4Format(videoData);
    }
    return nullptr;
}
