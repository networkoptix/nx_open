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

StreamInfo parseObjectStreamFile(const std::string& filePath, Issues* issues)
{
    const std::string jsonString = readFileToString(filePath);

    std::string error;
    const Json json = Json::parse(jsonString, error);

    if (!error.empty() && json.is_null())
    {
        issues->errors.insert(Issue::objectStreamIsNotAValidJson);
        return {};
    }

    if (!json.is_array())
    {
        issues->errors.insert(Issue::objectStreamIsNotAJsonArray);
        return {};
    }

    StreamInfo result;
    for (const Json& objectDescription: json.array_items())
    {
        if (!objectDescription.is_object())
        {
            issues->errors.insert(Issue::objectItemIsNotAJsonObject);
            continue;
        }

        Object object;
        if (!parseTrackId(objectDescription, &object, issues))
            continue;
        if (!parseCommonFields(objectDescription, &object, issues))
            continue;
        if (!parseBoundingBox(objectDescription, &object.boundingBox, issues))
            continue;
        if (!parseAttributes(objectDescription, &object.attributes, issues))
            continue;
        if (!parseTimestamp(objectDescription, &object.timestampUs, issues))
            continue;
        if (!parseImageSource(objectDescription, &object.imageSource, issues))
            continue;

        result.objectTypeIds.insert(object.typeId);
        result.objectsByFrameNumber[object.frameNumberToGenerateObject].push_back(
            std::move(object));
    }

    return result;
}

bool parseTrackId(
    const Json& objectDescription,
    Object* outObject,
    Issues* issues)
{
    if (!objectDescription[kTrackIdField].is_string())
    {
        issues->errors.insert(Issue::trackIdIsNotAString);
        return false;
    }

    const std::string& trackIdString = objectDescription[kTrackIdField].string_value();
    if (startsWith(trackIdString, kAutoTrackIdPerDeviceAgentCreationPrefix)
        || startsWith(trackIdString, kAutoTrackIdPerStreamCyclePrefix))
    {
        // Real track id will be generated later.
        outObject->trackIdRef = trackIdString;
        return true;
    }

    outObject->trackId = UuidHelper::fromStdString(objectDescription[kTrackIdField].string_value());
    if (outObject->trackId.isNull())
    {
        issues->errors.insert(Issue::trackIdIsNotAUuid);
        return false;
    }

    return true;
}

bool parseCommonFields(
    const Json& objectDescription,
    Object* outObject,
    Issues* issues)
{
    if (!objectDescription[kTypeIdField].is_string())
    {
        issues->errors.insert(Issue::typeIdIsNotAString);
        return false;
    }
    outObject->typeId = objectDescription[kTypeIdField].string_value();

    if (!objectDescription[kFrameNumberField].is_number())
    {
        issues->errors.insert(Issue::frameNumberIsNotANumber);
        return false;
    }
    outObject->frameNumberToGenerateObject = objectDescription[kFrameNumberField].int_value();

    outObject->entryType = Object::EntryType::regular;
    if (!objectDescription[kEntryTypeField].is_string())
    {
        issues->warnings.insert(Issue::objectEntryTypeIsNotAString);
    }
    else
    {
        const std::string& entryType = objectDescription[kEntryTypeField].string_value();
        if (entryType == kBestShotEntryType)
            outObject->entryType = Object::EntryType::bestShot;
        else if (entryType != kRegularEntryType && !entryType.empty())
            issues->warnings.insert(Issue::objectEntryTypeIsUnknown);
    }

    return true;
}

bool parseBoundingBox(
    const Json& objectDescription,
    Rect* outBoundingBox,
    Issues* issues)
{
    if (!objectDescription[kBoundingBoxField].is_object())
    {
        issues->errors.insert(Issue::boundingBoxIsNotAJsonObject);
        return false;
    }

    const Json boundingBox = objectDescription[kBoundingBoxField];
    if (!boundingBox[kTopLeftXField].is_number())
    {
        issues->errors.insert(Issue::topLeftXIsNotANumber);
        return false;
    }

    if (!boundingBox[kTopLeftYField].is_number())
    {
        issues->errors.insert(Issue::topLeftYIsNotANumber);
        return false;
    }

    if (!boundingBox[kWidthField].is_number())
    {
        issues->errors.insert(Issue::widthIsNotANumber);
        return false;
    }

    if (!boundingBox[kHeightField].is_number())
    {
        issues->errors.insert(Issue::heightIsNotANumber);
        return false;
    }

    outBoundingBox->x = (float) boundingBox[kTopLeftXField].number_value();
    outBoundingBox->y = (float) boundingBox[kTopLeftYField].number_value();
    outBoundingBox->width = (float) boundingBox[kWidthField].number_value();
    outBoundingBox->height = (float) boundingBox[kHeightField].number_value();

    if (outBoundingBox->x < 0 || outBoundingBox->y < 0
        || outBoundingBox->x + outBoundingBox->width > 1
        || outBoundingBox->y + outBoundingBox->height > 1)
    {
        issues->warnings.insert(Issue::objectIsOutOfBounds);
    }

    return true;
}

bool parseAttributes(
    const Json& objectDescription,
    std::map<std::string, std::string>* outAttributes,
    Issues* issues)
{
    if (objectDescription[kAttributesField].is_object())
    {
        const Json::object attributes = objectDescription[kAttributesField].object_items();
        for (const auto& item: attributes)
        {
            if (!item.second.is_string())
            {
                issues->warnings.insert(Issue::attributeValueIsNotAString);
                continue;
            }

            (*outAttributes)[item.first] = item.second.string_value();
        }
    }
    else if (!objectDescription[kAttributesField].is_object())
    {
        issues->warnings.insert(Issue::attributesFieldIsNotAJsonObject);
    }

    return true;
}

bool parseTimestamp(
    const Json& objectDescription,
    int64_t* outTimestampUs,
    Issues* issues)
{
    if (objectDescription[kTimestampUsField].is_number())
        *outTimestampUs = (int64_t) objectDescription[kTimestampUsField].number_value();
    else if (!objectDescription[kTimestampUsField].is_null())
        issues->warnings.insert(Issue::timestampIsNotANumber);

    return true;
}

bool parseImageSource(
    const Json& objectDescription,
    std::string* outImageSource,
    Issues*)
{
    if (!objectDescription[kEntryTypeField].is_string())
        return true;

    *outImageSource = objectDescription[kImageSourceField].string_value();
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
            return "Some items in the Object stream are not valid JSON objects";
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
        case Issue::objectEntryTypeIsNotAString:
            return "Entry type of some Items in the Object stream is not a string";
        case Issue::objectEntryTypeIsUnknown:
            return "Entry type of some Items in the Object stream is invalid";
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
