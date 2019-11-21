#include "point.h"

#include <charconv>

#include <nx/utils/log/log.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

PluginPoint::PluginPoint(const nx::kit::Json& json)
{
    auto& members = json.array_items();
    if (members.size() > 0)
        x = members[0].number_value();
    if (members.size() > 1)
        y = members[1].number_value();
}

std::ostream& PluginPoint::toSunapiStream(std::ostream& os, FrameSize frameSize) const
{
    os << int(x * frameSize.width + 0.5) << ',' << int(y * frameSize.height + 0.5);
    return os;
}

//-------------------------------------------------------------------------------------------------

std::optional<std::vector<PluginPoint>> parsePluginPoints(const char* value)
{
    NX_ASSERT(value);
    std::vector<PluginPoint> result;

    if (strcmp(value, "\"\"") == 0)
        return result; // Empty string means no figure.

    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(value, err);
    if (!json.is_object())
        return std::nullopt;

    const nx::kit::Json& jsonPoints = json["points"];
    if (!jsonPoints.is_array())
        return std::nullopt;

    std::vector<nx::kit::Json> jsonPointsAsArray = jsonPoints.array_items();
    result.reserve(jsonPointsAsArray.size());

    for (const auto &jsonPoint: jsonPointsAsArray)
        result.emplace_back(jsonPoint);
    return result;
}

std::optional<Direction> parsePluginDirection(const char* value)
{
    NX_ASSERT(value);
    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(value, err);
    if (!json.is_object())
        return std::nullopt;

    const nx::kit::Json& jsonPoints = json["direction"];
    if (!jsonPoints.is_string())
        return std::nullopt;

    std::string direction = jsonPoints.string_value();
    if (direction == "b")
        return Direction::Right;
    if (direction == "a")
        return Direction::Left;
    if (direction == "")
        return Direction::Both;

    return std::nullopt;
}

std::string pluginPointsToSunapiString(const std::vector<PluginPoint>& points, FrameSize frameSize)
{
    std::stringstream stream;
    if (!points.empty())
        points.front().toSunapiStream(stream, frameSize);
    for (size_t i = 1; i < points.size(); ++i)
        points[i].toSunapiStream(stream << ',', frameSize);
    return stream.str();
}

//-------------------------------------------------------------------------------------------------

std::vector<SunapiPoint> sunapiStringToSunapiPoints(const std::string& points)
{
    std::vector<SunapiPoint> result;
    if (points.empty())
        return result;

    const char* begin = &points.front();
    const char* end = &points.back() + 1;
    int x = 0;
    int y = 0;
    std::from_chars_result next{ begin, std::errc{} };
    while (next.ec == std::errc{} && next.ptr < end)
    {
        next = std::from_chars(next.ptr, end, x);
        ++next.ptr;
        next = std::from_chars(next.ptr, end, y);
        ++next.ptr;
        result.push_back(SunapiPoint(x, y));
    }
    return result;
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
