#include "setting_primitives_io.h"

#include <nx/utils/log/log.h>

#include <charconv>

using namespace std::literals;

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

namespace SettingPrimitivesServerIo
{

void deserializeOrThrow(const char* source, bool* destination)
{
    if (!source)
        throw DeserializationError();

    if (strcmp(source, "true") == 0)
    {
        *destination = true;
        return;
    }
    if (strcmp(source, "false") == 0)
    {
        *destination = false;
        return;
    }

    throw DeserializationError();
}

std::string serialize(bool value)
{
    return value ? "true"s : "false"s;

}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, int* destination)
{
    if (!source)
        throw DeserializationError();

    const char* end = source + strlen(source);
    std::from_chars_result conversionResult = std::from_chars(source, end, *destination);
    if (conversionResult.ptr == end)
        return;

    throw DeserializationError();
}

std::string serialize(int value)
{
    return std::to_string(value);
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, std::string* destination)
{
    if (!source)
        throw DeserializationError();

    *destination = source;
}

std::string serialize(std::string value)
{
    return value;
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, ObjectSizeConstraints* destination)
{
    if (!source)
        throw DeserializationError();

    std::optional<ObjectSizeConstraints> tmp = ObjectSizeConstraints::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw DeserializationError();
}

std::string serialize(const ObjectSizeConstraints& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, NamedPolygon* destination)
{
    if (!source)
        throw DeserializationError();

    std::optional<NamedPolygon> tmp = NamedPolygon::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw DeserializationError();
}

std::string serialize(const NamedPolygon& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, UnnamedPolygon* destination)
{
    if (!source)
        throw DeserializationError();

    std::optional<UnnamedPolygon> tmp = UnnamedPolygon::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw DeserializationError();
}

std::string serialize(const UnnamedPolygon& value)
{
    return value.toServerString();
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const char* source, NamedLineFigure* destination)
{
    if (!source)
        throw DeserializationError();

    std::optional<NamedLineFigure> tmp = NamedLineFigure::fromServerString(source);

    if (tmp)
    {
        *destination = *tmp;
        return;
    }

    throw DeserializationError();
}

std::string serialize(const NamedLineFigure& value)
{
    return value.toServerString();
}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

namespace SettingPrimitivesDeviceIo
{

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, bool* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_bool())
        *result = param.bool_value();
    else
        throw CameraResponseJsonError{};
}

std::string serialize(bool value)
{
    return value ? "True"s : "False"s;
}

//-------------------------------------------------------------------------------------------------

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, int* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_number())
        *result = param.int_value();
    else
        throw CameraResponseJsonError{};
}

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, std::string* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_string())
        *result = param.string_value();
    else
        throw CameraResponseJsonError{};
}

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, PluginPoint* result)
{
    NX_ASSERT(key);

    const auto& param = json[key];
    if (!param.is_string())
        throw CameraResponseJsonError{};

    const std::string value = param.string_value();

    if (!result->fromSunapiString(value, frameSize))
        throw CameraResponseJsonError();
}

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, std::vector<PluginPoint>* result)
{
    NX_ASSERT(key);

    const auto& points = json[key];
    if (!points.is_array())
        throw CameraResponseJsonError{};

    result->reserve(points.array_items().size());
    for (const nx::kit::Json& point : points.array_items())
    {
        if (!point["x"].is_number() || !point["y"].is_number())
            throw CameraResponseJsonError{};

        const int ix = point["x"].int_value();
        const int iy = point["y"].int_value();
        result->emplace_back(frameSize.xAbsoluteToRelative(ix), frameSize.yAbsoluteToRelative(iy));
    }
}

void deserializeOrThrow(const nx::kit::Json& json, FrameSize frameSize, ObjectSizeConstraints* result)
{
    PluginPoint minSize, maxSize;
    deserializeOrThrow(json, "MinimumObjectSizeInPixels", frameSize, &minSize);
    deserializeOrThrow(json, "MaximumObjectSizeInPixels", frameSize, &maxSize);
    result->minWidth = minSize.x;
    result->minHeight = minSize.y;
    result->maxWidth = maxSize.x;
    result->maxHeight = maxSize.y;
}

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize frameSize, UnnamedPolygon* result)
{
    std::vector<PluginPoint> points;
    deserializeOrThrow(json, key, frameSize, &points);
    result->points = points;
}

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, Direction* result)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_string())
    {
        // Different firmware versions have different valid value sets for line crossing direction.
        if ((param.string_value() == "RightSide") || (param.string_value() == "Right"))
            *result = Direction::Right;
        else if ((param.string_value() == "LeftSide") || (param.string_value() == "Left"))
            *result = Direction::Left;
        else if (param.string_value() == "BothDirections")
            *result = Direction::Both;
        else if (param.string_value() != "Off")
            throw CameraResponseJsonError{}; //< unknown direction
    }
    else
        throw CameraResponseJsonError{};
}

void deserializeOrThrow(const nx::kit::Json& json, const char* key, FrameSize /*frameSize*/, bool* result, const char* desired)
{
    NX_ASSERT(key);

    if (const auto& param = json[key]; param.is_array())
    {
        *result = false;
        for (const nx::kit::Json& item : param.array_items())
        {
            if (item.string_value() == desired)
            {
                *result = true;
                break;
            }
        }
    }
    else
        throw CameraResponseJsonError{};
}

std::string serialize(const std::vector<PluginPoint>& points, FrameSize frameSize)
{
    std::stringstream stream;
    if (!points.empty())
        points.front().toSunapiStream(stream, frameSize);
    for (size_t i = 1; i < points.size(); ++i)
        points[i].toSunapiStream(stream << ',', frameSize);
    return stream.str();
}

}

} // namespace nx::vms_server_plugins::analytics::hanwha
