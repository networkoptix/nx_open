// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "to_string.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

namespace nx::sdk {

static const int kLevelIndent = 4;

static std::string indent(int overallIndent, int level)
{
    return std::string(overallIndent, ' ') + std::string(kLevelIndent * level, ' ');
}

std::string toString(const IString* string)
{
    if (!string)
        return std::string();

    return std::string(string->str());
}

std::string toString(const IStringMap* map, int overallIndent)
{
    if (!map || map->count() == 0)
        return std::string();

    std::string result;
    for (int i = 0; i < map->count(); ++i)
    {
        result += indent(overallIndent, 0)
            + "[" + nx::kit::utils::toString(map->key(i)) + "]: "
            + nx::kit::utils::toString(map->value(i)) + "\n";
    }

    return result;
}

std::string toJsonString(const IStringMap* map, int overallIndent)
{
    if (!map)
    {
        NX_KIT_ASSERT(map);
        return "[]\n";
    }

    std::string result = indent(overallIndent, 0) + "[\n";

    const int count = map->count();
    for (int i = 0; i < count; ++i)
    {
        result += indent(overallIndent, 1) + "{ "
            + "\"name\": " + nx::kit::utils::toString(map->key(i)) + ", "
            + "\"value\": " + nx::kit::utils::toString(map->value(i))
            + " }" + ((i < count - 1) ? "," : "") + "\n";
    }

    result += indent(overallIndent, 0) + "]";
    return result;
}

std::string toJsonString(const IDeviceInfo* deviceInfo, int overallIndent)
{
    using nx::kit::utils::toString;

    std::string result = indent(overallIndent, 0) + "{\n";

    const std::string innerIndent = indent(overallIndent, 1);

    result += innerIndent + "\"id\": " + toString(deviceInfo->id()) + ",\n";
    result += innerIndent + "\"vendor\": " + toString(deviceInfo->vendor()) + ",\n";
    result += innerIndent + "\"model\": " + toString(deviceInfo->model()) + ",\n";
    result += innerIndent + "\"firmware\": " + toString(deviceInfo->firmware()) + ",\n";
    result += innerIndent + "\"name\": " + toString(deviceInfo->name()) + ",\n";
    result += innerIndent + "\"url\": " + toString(deviceInfo->url()) + ",\n";
    result += innerIndent + "\"login\": " + toString(deviceInfo->login()) + ",\n";
    result += innerIndent + "\"channelNumber\": " + toString(deviceInfo->channelNumber()) + ",\n";
    result += innerIndent + "\"_password\": \"_hidden_\",\n";
    result += innerIndent + "\"sharedId\": " + toString(deviceInfo->sharedId()) + ",\n";
    result += innerIndent + "\"logicalId\": " + toString(deviceInfo->logicalId()) + "\n";

    result += indent(overallIndent, 0) + "}";
    return result;
}

std::string toString(ErrorCode errorCode)
{
    switch (errorCode)
    {
        case ErrorCode::noError: return "noError";
        case ErrorCode::otherError: return "otherError";
        case ErrorCode::networkError: return "networkError";
        case ErrorCode::unauthorized: return "unauthorized";
        case ErrorCode::internalError: return "internalError";
        case ErrorCode::invalidParams: return "invalidParams";
        case ErrorCode::notImplemented: return "notImplemented";
        default: return "<unsupported Error>";
    }
}

} // namespace nx::sdk
