#include "point.h"

#include <charconv>
#include <cmath>

#include <nx/utils/log/log.h>

using namespace std::string_literals;

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
    os << frameSize.xRelativeToAbsolute(x) << ',' << frameSize.yRelativeToAbsolute(y);
    return os;
}

//-------------------------------------------------------------------------------------------------

nx::kit::Json PluginPoint::toJson() const
{
    nx::kit::Json jx(x);
    nx::kit::Json jy(y);
    nx::kit::Json::array coordArray{ jx, jy };
    return nx::kit::Json(coordArray);
}

//-------------------------------------------------------------------------------------------------
bool PluginPoint::fromSunapiString(const char*& begin, const char* end, FrameSize frameSize)
{
    // correct data is {int, comma, int}

    int ix = 0;
    int iy = 0;
    std::from_chars_result conversionResult = std::from_chars(begin, end, ix);
    begin = conversionResult.ptr;
    if (conversionResult.ec != std::errc())
        return false; // non an int
    if (begin == end || *begin != ',')
        return false; // not a comma
    ++begin;
    conversionResult = std::from_chars(begin, end, iy);
    begin = conversionResult.ptr;
    if (conversionResult.ec != std::errc())
        return false; // non an int

    this->x = frameSize.xAbsoluteToRelative(ix);
    this->y = frameSize.yAbsoluteToRelative(iy);
    return true;
}

bool PluginPoint::fromSunapiString(const std::string& value, FrameSize frameSize)
{
    const char* begin = value.c_str();
    return fromSunapiString(begin, begin + value.length(), frameSize);
}

std::istream& PluginPoint::fromSunapiStream(std::istream& is, FrameSize frameSize)
{
    int ix = 0;
    int iy = 0;
    is >> ix;
    is.ignore();
    is >> iy;
    this->x = frameSize.xAbsoluteToRelative(ix);
    this->y = frameSize.yAbsoluteToRelative(iy);
    return is;
}
//-------------------------------------------------------------------------------------------------
std::optional<std::vector<PluginPoint>> ServerStringToPluginPoints(const char* value)
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

std::optional<Width> ServerStringToWidth(const char* value)
{
    const std::optional<std::vector<PluginPoint>> box = ServerStringToPluginPoints(value);
    if (!box || box->size() != 2)
        return std::nullopt;
    return Width{ std::abs((*box)[0].x - (*box)[1].x) };
}

std::optional<Height> ServerStringToHeight(const char* value)
{
    const std::optional<std::vector<PluginPoint>> box = ServerStringToPluginPoints(value);
    if (!box || box->size() != 2)
        return std::nullopt;
    return Height{ std::abs((*box)[0].y - (*box)[1].y) };
}

/** Make a vector of two points that set a centered rectangle with definite width and height. */
std::optional<std::vector<PluginPoint>> WidthHeightToPluginPoints(Width width, Height height)
{
    NX_ASSERT(width <= 1.0 && height <= 1.0);
    std::vector<PluginPoint> result;

    const double x0 = (1.0 - width) / 2.0;
    const double x1 = x0 + width;

    const double y0 = (1.0 - height) / 2.0;
    const double y1 = y0 + height;

    result.emplace_back(x0, y0);
    result.emplace_back(x1, y1);
    return result;
}

std::optional<Direction> ServerStringToDirection(const char* value)
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

std::string pluginPointsToServerJson(const std::vector<PluginPoint>& points)
{
    nx::kit::Json::array jPointArray;
    jPointArray.reserve(points.size());
    for (const auto& point: points)
    {
        nx::kit::Json jPoint = point.toJson();
        jPointArray.push_back(jPoint);
    }

    nx::kit::Json::object m;
    m.insert(std::pair("points"s, jPointArray));

    nx::kit::Json jo(m);
    std::string result = jo.dump();
    return result;

}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
