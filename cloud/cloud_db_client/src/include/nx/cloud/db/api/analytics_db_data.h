// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/utils/url_query.h>
#include <nx/utils/uuid.h>

namespace nx::cloud::db::api {

struct BoundingBoxData
{
    float x = 0.0;
    float y = 0.0;
    float width = 0.0;
    float height = 0.0;
};
NX_REFLECTION_INSTRUMENT(BoundingBoxData, (x)(y)(width)(height))

struct TrackImageData
{
    QByteArray data;
    std::string mimeType;
};
NX_REFLECTION_INSTRUMENT(TrackImageData, (data)(mimeType))

struct ObjectTrackVideoFrameImageInfoData
{
    std::optional<BoundingBoxData> boundingBox;
    std::optional<TrackImageData> image;
    int streamIndex = 0;
    std::chrono::milliseconds timestampMs{0};
};
NX_REFLECTION_INSTRUMENT(ObjectTrackVideoFrameImageInfoData, (boundingBox)(image)(streamIndex)(timestampMs))
NX_REFLECTION_TAG_TYPE(ObjectTrackVideoFrameImageInfoData, jsonSerializeChronoDurationAsNumber)

struct TitleData
{
    std::optional<std::string> text;
    std::optional<ObjectTrackVideoFrameImageInfoData> imageInfo;
};
NX_REFLECTION_INSTRUMENT(TitleData, (text)(imageInfo))

struct TitleWithFlagData
{
    std::optional<std::string> text;
    std::optional<ObjectTrackVideoFrameImageInfoData> imageInfo;
    bool isImageAvailable = false;
};
NX_REFLECTION_INSTRUMENT(TitleWithFlagData, (text)(imageInfo)(isImageAvailable))

struct TrackData
{
    nx::Uuid id;
    nx::Uuid deviceId;
    std::string siteId;
    nx::Uuid engineId;
    std::string objectTypeId;
    std::chrono::milliseconds startTimeMs;
    std::chrono::milliseconds endTimeMs;
    std::map<std::string, std::string> attributes;
    QByteArray objectRegion;
    std::optional<ObjectTrackVideoFrameImageInfoData> bestShot;
    std::optional<QByteArray> vector;
};
#define TrackData_Fields (id)(deviceId)(siteId)(engineId)(objectTypeId)(startTimeMs)(endTimeMs)(attributes)(objectRegion)(bestShot)(vector)
NX_REFLECTION_INSTRUMENT(TrackData, TrackData_Fields)
NX_REFLECTION_TAG_TYPE(TrackData, jsonSerializeChronoDurationAsNumber)

struct SaveTrackData: public TrackData
{
    std::optional<TitleData> title;
};
NX_REFLECTION_INSTRUMENT(SaveTrackData, TrackData_Fields(title))
NX_REFLECTION_TAG_TYPE(SaveTrackData, jsonSerializeChronoDurationAsNumber)

struct ReadTrackData: public TrackData
{
    std::optional<TitleWithFlagData> title;
};
NX_REFLECTION_INSTRUMENT(ReadTrackData, TrackData_Fields(title))
NX_REFLECTION_TAG_TYPE(ReadTrackData, jsonSerializeChronoDurationAsNumber)

using SaveTracksData = std::vector<SaveTrackData>;
using ReadTracksData = std::vector<ReadTrackData>;

struct GetTracksParamsData
{
    std::set<nx::Uuid> deviceIds;
    std::set<std::string> objectTypeIds;
    std::optional<int64_t> startTimeMs;      //< UTC datetime.
    std::optional<int64_t> endTimeMs;    //< UTC datetime.
    std::optional<std::string> boundingBox;  //< e.g. "{x},{y},{width}x{height}".
    std::string freeText;
    std::string textScope;
    nx::Uuid referenceBestShotId;
    nx::Uuid engineId;
    std::string siteId;
    int limit = 0;
    int offset = 0;

    nx::UrlQuery toUrlQuery() const;
};

} // namespace nx::cloud::db::api
