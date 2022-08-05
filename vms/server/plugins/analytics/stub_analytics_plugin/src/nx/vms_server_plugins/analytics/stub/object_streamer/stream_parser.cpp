// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_parser.h"

#include "constants.h"
#include "utils.h"
#include "../utils.h"

#include <nx/kit/debug.h>
#include <nx/sdk/helpers/uuid_helper.h>

using nx::kit::Json;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

StreamInfo parseObjectStreamFile(const std::string& filePath, Issues* outIssues)
{
    const std::string jsonString = readFileToString(filePath);

    std::string error;
    const Json json = Json::parse(jsonString, error);

    if (!error.empty() && json.is_null())
    {
        outIssues->errors.insert(Issue::objectStreamIsNotAValidJson);
        return {};
    }

    if (!json.is_array())
    {
        outIssues->errors.insert(Issue::objectStreamIsNotAJsonArray);
        return {};
    }

    StreamInfo result;
    for (const Json& objectDescription: json.array_items())
    {
        if (!objectDescription.is_object())
        {
            outIssues->errors.insert(Issue::objectItemIsNotAJsonObject);
            continue;
        }

        Object object;
        if (!parseCommonFields(objectDescription, &object, outIssues))
            continue;
        if (!parseBoundingBox(objectDescription, &object.boundingBox, outIssues))
            continue;
        if (!parseAttributes(objectDescription, &object.attributes, outIssues))
            continue;
        if (!parseTimestamp(objectDescription, &object.timestampUs, outIssues))
            continue;

        result.objectTypeIds.insert(object.typeId);
        result.objectsByFrameNumber[object.frameNumberToGenerateObject].push_back(
            std::move(object));
    }

    return result;
}

bool parseCommonFields(
    const nx::kit::Json& objectDescription,
    Object* outObject,
    Issues* outIssues)
{
    if (!objectDescription[kTypeIdField].is_string())
    {
        outIssues->errors.insert(Issue::typeIdIsNotAString);
        return false;
    }

    outObject->typeId = objectDescription[kTypeIdField].string_value();

    if (!objectDescription[kTrackIdField].is_string())
    {
        outIssues->errors.insert(Issue::trackIdIsNotAString);
        return false;
    }

    outObject->trackId = UuidHelper::fromStdString(objectDescription[kTrackIdField].string_value());
    if (outObject->trackId.isNull())
    {
        outIssues->errors.insert(Issue::trackIdIsNotAUuid);
        return false;
    }

    if (!objectDescription[kFrameNumberField].is_number())
    {
        outIssues->errors.insert(Issue::frameNumberIsNotANumber);
        return false;
    }

    outObject->frameNumberToGenerateObject = objectDescription[kFrameNumberField].int_value();

    return true;
}

bool parseBoundingBox(
    const Json& objectDescription,
    Rect* outBoundingBox,
    Issues* outIssues)
{
    if (!objectDescription[kBoundingBoxField].is_object())
    {
        outIssues->errors.insert(Issue::boundingBoxIsNotAJsonObject);
        return false;
    }

    const Json boundingBox = objectDescription[kBoundingBoxField];
    if (!boundingBox[kTopLeftXField].is_number())
    {
        outIssues->errors.insert(Issue::topLeftXIsNotANumber);
        return false;
    }

    if (!boundingBox[kTopLeftYfield].is_number())
    {
        outIssues->errors.insert(Issue::topLeftYIsNotANumber);
        return false;
    }

    if (!boundingBox[kWidthField].is_number())
    {
        outIssues->errors.insert(Issue::widthIsNotANumber);
        return false;
    }

    if (!boundingBox[kHeightField].is_number())
    {
        outIssues->errors.insert(Issue::heightIsNotANumber);
        return false;
    }

    outBoundingBox->x = (float) boundingBox[kTopLeftXField].number_value();
    outBoundingBox->y = (float) boundingBox[kTopLeftYfield].number_value();
    outBoundingBox->width = (float) boundingBox[kWidthField].number_value();
    outBoundingBox->height = (float) boundingBox[kHeightField].number_value();

    if (outBoundingBox->x < 0 || outBoundingBox->y < 0
        || outBoundingBox->x + outBoundingBox->width > 1
        || outBoundingBox->y + outBoundingBox->height > 1)
    {
        outIssues->warnings.insert(Issue::objectIsOutOfBounds);
    }

    return true;
}

bool parseAttributes(
    const Json& objectDescription,
    std::map<std::string, std::string>* outAttributes,
    Issues* outIssues)
{
    if (objectDescription[kAttributesField].is_object())
    {
        const Json::object attributes = objectDescription[kAttributesField].object_items();
        for (const auto& item: attributes)
        {
            if (!item.second.is_string())
            {
                outIssues->warnings.insert(Issue::attributeValueIsNotAString);
                continue;
            }

            (*outAttributes)[item.first] = item.second.string_value();
        }
    }
    else if (!objectDescription[kAttributesField].is_object())
    {
        outIssues->warnings.insert(Issue::attributesFieldIsNotAJsonObject);
    }

    return true;
}

bool parseTimestamp(
    const nx::kit::Json& objectDescription,
    int64_t* outTimestampUs,
    Issues* outIssues)
{
    if (objectDescription[kTimestampUsField].is_number())
        *outTimestampUs = (int64_t) objectDescription[kTimestampUsField].number_value();
    else if (!objectDescription[kTimestampUsField].is_null())
        outIssues->warnings.insert(Issue::timestampIsNotANumber);

    return true;
}

std::string issueToString(Issue issue)
{
    switch (issue)
    {
        case Issue::objectStreamIsNotAValidJson:
            return "Object stream file contains invalid JSON";
        case Issue::objectStreamIsNotAJsonArray:
            return "Object stream must be a valid JSON array";
        case Issue::objectItemIsNotAJsonObject:
            return "Some of items in the Object stream are not valid JSON objects";
        case Issue::trackIdIsNotAString:
            return "Track id of some items in the Object stream is not a string";
        case Issue::trackIdIsNotAUuid:
            return "Track id of some items in the Object stream is not a valid UUID";
        case Issue::typeIdIsNotAString:
            return "Type id of some items in the Object stream is not a string";
        case Issue::frameNumberIsNotANumber:
            return "Frame number of some items in the Object stream is not a number";
        case Issue::boundingBoxIsNotAJsonObject:
            return "Bounding box of some items in the Object stream is not a valid JSON object";
        case Issue::topLeftXIsNotANumber:
            return "Bounding box X coordinate of some items in the Object stream is not a number";
        case Issue::topLeftYIsNotANumber:
            return "Bounding box Y coordinate of some items in the Object stream is not a number";
        case Issue::widthIsNotANumber:
            return "Bounding box width of some items in the Object stream is not a number";
        case Issue::heightIsNotANumber:
            return "Bounding box height coordinate of some items in the Object stream is not a number";
        case Issue::objectIsOutOfBounds:
            return "Bounding box of some items in the Object stream is out of bounds";
        case Issue::attributesFieldIsNotAJsonObject:
            return "Attribute field of some items in the Object stream is not a valid JSON object";
        case Issue::attributeValueIsNotAString:
            return "Attribute values of some items in the Object stream is not a string";
        case Issue::timestampIsNotANumber:
            return "Timestamp of some items in the Object stream is not a number";
        default:
            NX_KIT_ASSERT(false, "Unexpected issue");
            return {};
    };
}

} // namespace object_streamer
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
