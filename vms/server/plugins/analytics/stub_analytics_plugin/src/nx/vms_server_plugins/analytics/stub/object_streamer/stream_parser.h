// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include <nx/kit/json.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

struct Object
{
    enum class EntryType
    {
        regular,
        bestShot,
    };

    std::string typeId;
    nx::sdk::Uuid trackId;
    std::string trackIdRef;
    nx::sdk::analytics::Rect boundingBox;
    std::map<std::string, std::string> attributes;
    int frameNumberToGenerateObject = 0;
    int64_t timestampUs = -1;
    EntryType entryType = EntryType::regular;
    std::string imageSource;
};

enum class Issue
{
    objectStreamIsNotAValidJson,
    objectStreamIsNotAJsonArray,
    objectItemIsNotAJsonObject,
    trackIdIsNotAString,
    trackIdIsNotAUuid,
    typeIdIsNotAString,
    frameNumberIsNotANumber,
    boundingBoxIsNotAJsonObject,
    topLeftXIsNotANumber,
    topLeftYIsNotANumber,
    widthIsNotANumber,
    heightIsNotANumber,
    objectIsOutOfBounds,
    attributesFieldIsNotAJsonObject,
    attributeValueIsNotAString,
    timestampIsNotANumber,
};

struct Issues
{
    std::set<Issue> errors;
    std::set<Issue> warnings;
};

struct StreamInfo
{
    std::map<int, std::vector<Object>> objectsByFrameNumber;
    std::set<std::string> objectTypeIds;
};

StreamInfo parseObjectStreamFile(const std::string& filePath, Issues* outIssues);

bool parseTrackId(
    const nx::kit::Json& objectDescription,
    Object* outObject,
    Issues* outIssues);

bool parseCommonFields(
    const nx::kit::Json& objectDescription,
    Object* outObject,
    Issues* outIssues);

bool parseBoundingBox(
    const nx::kit::Json& objectDescription,
    nx::sdk::analytics::Rect* outBoundingBox,
    Issues* outIssues);

bool parseAttributes(
    const nx::kit::Json& objectDescription,
    std::map<std::string, std::string>* outAttributes,
    Issues* outIssues);

bool parseTimestamp(
    const nx::kit::Json& objectDescription,
    int64_t* outTimestamp,
    Issues* outIssues);

bool parseImageSource(
    const nx::kit::Json& objectDescription,
    std::string* outImageSource,
    Issues* outIssues);

std::string issueToString(Issue issue);

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
