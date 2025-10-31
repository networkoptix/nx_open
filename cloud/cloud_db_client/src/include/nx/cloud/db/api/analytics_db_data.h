// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/url_query.h>
#include <nx/utils/uuid.h>

#include "time_period_data.h"

namespace nx::cloud::db::api {

struct BoundingBoxData
{
    float x = 0.0;
    float y = 0.0;
    float width = 0.0;
    float height = 0.0;

    bool operator==(const BoundingBoxData&) const = default;
};
NX_REFLECTION_INSTRUMENT(BoundingBoxData, (x)(y)(width)(height))

struct TrackImageData
{
    QByteArray data;
    std::string mimeType;

    bool operator==(const TrackImageData&) const = default;
};
NX_REFLECTION_INSTRUMENT(TrackImageData, (data)(mimeType))

struct ObjectTrackVideoFrameImageInfoData
{
    std::optional<BoundingBoxData> boundingBox;
    std::optional<TrackImageData> image;
    int streamIndex = 0;
    std::chrono::milliseconds timestampMs{0};

    bool operator==(const ObjectTrackVideoFrameImageInfoData&) const = default;
};
NX_REFLECTION_INSTRUMENT(ObjectTrackVideoFrameImageInfoData, (boundingBox)(image)(streamIndex)(timestampMs))
NX_REFLECTION_TAG_TYPE(ObjectTrackVideoFrameImageInfoData, jsonSerializeChronoDurationAsNumber)

struct TitleData
{
    std::optional<std::string> text;
    std::optional<ObjectTrackVideoFrameImageInfoData> imageInfo;

    bool operator==(const TitleData&) const = default;
};
NX_REFLECTION_INSTRUMENT(TitleData, (text)(imageInfo))

struct TitleWithFlagData
{
    std::optional<std::string> text;
    std::optional<ObjectTrackVideoFrameImageInfoData> imageInfo;
    bool isImageAvailable = false;

    bool operator==(const TitleWithFlagData&) const = default;
};
NX_REFLECTION_INSTRUMENT(TitleWithFlagData, (text)(imageInfo)(isImageAvailable))

struct VectorData
{
    QByteArray data;
    std::string model;

    bool operator==(const VectorData&) const = default;
};
#define VectorData_Fields (data)(model)
NX_REFLECTION_INSTRUMENT(VectorData, VectorData_Fields)

struct TrackData
{
    std::string organizationId;
    std::string siteId;
    nx::Uuid id;
    nx::Uuid deviceId;
    nx::Uuid engineId;
    std::string objectTypeId;
    std::chrono::milliseconds startTimeMs;
    std::chrono::milliseconds endTimeMs;
    std::map<std::string, std::string> attributes;
    QByteArray objectRegion;
    std::optional<ObjectTrackVideoFrameImageInfoData> bestShot;
    std::optional<VectorData> vector;

    bool operator==(const TrackData&) const = default;
};
#define TrackData_Fields (organizationId)(siteId)(id)(deviceId)(engineId)(objectTypeId)(startTimeMs)(endTimeMs)(attributes)(objectRegion)(bestShot)(vector)
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
    std::optional<float> similarity;

    bool operator==(const ReadTrackData&) const = default;
};
NX_REFLECTION_INSTRUMENT(ReadTrackData, TrackData_Fields(title)(similarity))
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
    std::string organizationId;
    std::string siteId;
    int limit = 0;
    int offset = 0;

    nx::UrlQuery toUrlQuery() const;
};

struct TaxonomyData
{
    std::string version;
    QJsonObject descriptors;

    bool operator==(const TaxonomyData&) const = default;
};
NX_REFLECTION_INSTRUMENT(TaxonomyData, (version)(descriptors))

} // namespace nx::cloud::db::api
