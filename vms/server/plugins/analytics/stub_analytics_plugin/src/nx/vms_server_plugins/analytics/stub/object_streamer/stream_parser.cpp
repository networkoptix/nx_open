// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_parser.h"

#include "constants.h"
#include "utils.h"
#include "../utils.h"

#include <nx/kit/debug.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "stub_analytics_plugin_object_streamer_ini.h"

using nx::kit::Json;
using nx::kit::jsonTypeToString;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_streamer {

/** Can be called as `return addError(...);`. */
[[maybe_unused]] static bool addError(
    Issues* issues, Issue issue, const std::string& context = "")
{
    issues->errors.insert(issue);
    NX_PRINT << "Issue (error): " << issueToString(issue)
        << (context.empty()? "" : (". Context: " + context));
    return false;
}

/** Can be called as `return addWarning(...);`. */
[[maybe_unused]] static bool addWarning(
    Issues* issues, Issue issue, const std::string& context = "")
{
    issues->warnings.insert(issue);
    NX_PRINT << "Issue (warning): " << issueToString(issue)
        << (context.empty()? "" : (". Context: " + context));
    return true;
}

static std::string dumpRect(const Rect& rect)
{
    return "Rect(x: " + std::to_string(rect.x) + ", y: " + std::to_string(rect.y)
        + ", width: " + std::to_string(rect.width) + ", height: " + std::to_string(rect.height)
        + ")";
}

/** Performs C-escaping to reveal control characters and other potential issues in the JSON. */
static std::string dumpJson(const Json& json)
{
    static const int kMaxJsonDumpSize = 99; //< Too long JSON values will be truncated in the log.
    std::string dump = json.dump();
    if (dump.size() > kMaxJsonDumpSize)
        dump = dump.substr(0, kMaxJsonDumpSize) + "...";
    return "JSON " + jsonTypeToString(json.type()) + " " + nx::kit::utils::toString(dump);
}

/** Performs C-escaping to reveal control characters and other potential issues in the string. */
static std::string dumpString(const std::string& s)
{
    return nx::kit::utils::toString(s);
}

StreamInfo parseObjectStreamFile(const std::string& filePath, Issues* issues)
{
    const std::string jsonString = readFileToString(filePath);

    std::string error;
    const Json json = Json::parse(jsonString, error);

    if (!error.empty() && json.is_null())
    {
        addError(issues, Issue::objectStreamIsNotAValidJson);
        return {};
    }

    if (!json.is_array())
    {
        addError(issues, Issue::objectStreamIsNotAJsonArray, dumpJson(json));
        return {};
    }

    StreamInfo result;
    for (const Json& objectDescription: json.array_items())
    {
        if (!objectDescription.is_object())
        {
            addError(issues, Issue::objectItemIsNotAJsonObject, dumpJson(objectDescription));
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
    const Json& trackId = objectDescription[kTrackIdField];
    if (!trackId.is_string())
        return addError(issues, Issue::trackIdIsNotAString, dumpJson(trackId));

    const std::string& trackIdString = trackId.string_value();
    if (startsWith(trackIdString, kAutoTrackIdPerDeviceAgentCreationPrefix)
        || startsWith(trackIdString, kAutoTrackIdPerStreamCyclePrefix))
    {
        // Real track id will be generated later.
        outObject->trackIdRef = trackIdString;
        return true;
    }

    outObject->trackId = UuidHelper::fromStdString(trackIdString);
    if (outObject->trackId.isNull())
        return addError(issues, Issue::trackIdIsNotAUuid, dumpString(trackIdString));

    return true;
}

bool parseCommonFields(
    const Json& objectDescription,
    Object* outObject,
    Issues* issues)
{
    const Json& typeId = objectDescription[kTypeIdField];
    if (!typeId.is_string())
        return addError(issues, Issue::typeIdIsNotAString, dumpJson(typeId));
    outObject->typeId = typeId.string_value();

    const Json& frameNumber = objectDescription[kFrameNumberField];
    if (!frameNumber.is_number())
        return addError(issues, Issue::frameNumberIsNotANumber, dumpJson(frameNumber));
    outObject->frameNumberToGenerateObject = frameNumber.int_value();

    outObject->entryType = Object::EntryType::regular;
    const Json& entryType = objectDescription[kEntryTypeField];
    if (entryType.is_string())
    {
        const std::string& entryTypeString = entryType.string_value();
        if (entryTypeString == kBestShotEntryType)
            outObject->entryType = Object::EntryType::bestShot;
        else if (entryTypeString != kRegularEntryType && !entryTypeString.empty())
            addWarning(issues, Issue::objectEntryTypeIsUnknown, dumpString(entryTypeString));
    }
    else if (!entryType.is_null())
    {
        addWarning(issues, Issue::objectEntryTypeIsNotAString, dumpJson(entryType));
    }
    return true;
}

bool parseBoundingBox(
    const Json& objectDescription,
    Rect* outBoundingBox,
    Issues* issues)
{
    const Json& boundingBox = objectDescription[kBoundingBoxField];
    if (!boundingBox.is_object())
        return addError(issues, Issue::boundingBoxIsNotAJsonObject, dumpJson(boundingBox));

    const auto& parseParam =
        [&boundingBox, &issues](float* outValue, const std::string& fieldName, Issue issue) -> bool
        {
            const Json& value = boundingBox[fieldName];
            if (!value.is_number())
                return addError(issues, issue, dumpJson(value));
            *outValue = (float) value.number_value();
            return true;
        };

    if (!parseParam(&outBoundingBox->x, kTopLeftXField, Issue::topLeftXIsNotANumber)
        || !parseParam(&outBoundingBox->y, kTopLeftYField, Issue::topLeftYIsNotANumber)
        || !parseParam(&outBoundingBox->width, kWidthField, Issue::widthIsNotANumber)
        || !parseParam(&outBoundingBox->height, kHeightField, Issue::heightIsNotANumber))
    {
        return false;
    }

    if (outBoundingBox->x < 0 || outBoundingBox->y < 0
        || outBoundingBox->x + outBoundingBox->width > 1
        || outBoundingBox->y + outBoundingBox->height > 1)
    {
        addWarning(issues, Issue::objectIsOutOfBounds, dumpRect(*outBoundingBox));
    }

    return true;
}

bool parseAttributes(
    const Json& objectDescription,
    std::map<std::string, std::string>* outAttributes,
    Issues* issues)
{
    const Json& attributes = objectDescription[kAttributesField];
    if (!attributes.is_object())
        return addWarning(issues, Issue::attributesFieldIsNotAJsonObject, dumpJson(attributes));

    for (const auto& item: attributes.object_items())
    {
        if (!item.second.is_string())
        {
            addWarning(issues, Issue::attributeValueIsNotAString, dumpJson(item.second));
            continue;
        }

        (*outAttributes)[item.first] = item.second.string_value();
    }

    return true;
}

bool parseTimestamp(
    const Json& objectDescription,
    int64_t* outTimestampUs,
    Issues* issues)
{
    const Json& timestampUs = objectDescription[kTimestampUsField];
    if (timestampUs.is_number())
        *outTimestampUs = (int64_t) timestampUs.number_value();
    else if (!timestampUs.is_null())
        addWarning(issues, Issue::timestampIsNotANumber, dumpJson(timestampUs));

    return true;
}

bool parseImageSource(
    const Json& objectDescription,
    std::string* outImageSource,
    Issues* issues)
{
    const Json& imageSource = objectDescription[kImageSourceField];
    if (imageSource.is_string())
        *outImageSource = objectDescription[kImageSourceField].string_value();
    else if (!imageSource.is_null())
        addWarning(issues, Issue::imageSourceIsNotAString);

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
        case Issue::imageSourceIsNotAString:
            return "Image source of some Items in the Object stream is not a string";
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
