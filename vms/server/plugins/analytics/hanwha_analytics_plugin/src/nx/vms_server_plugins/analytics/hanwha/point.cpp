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
std::optional<std::vector<PluginPoint>> ServerStringToPluginPoints(
    const char* source, std::string* label /*= nullptr*/, Direction* direction /*= nullptr*/)
{
    NX_ASSERT(source);
    std::vector<PluginPoint> result;

    if (strcmp(source, "null") == 0)
    {
        if (label)
            *label = std::string();
        if (direction)
            *direction = Direction();
        return result; //< Empty figure.
    }

    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(source, err);
    if (!json.is_object())
        return std::nullopt;

    if (const nx::kit::Json& jsonFigure = json["figure"]; !jsonFigure.is_object())
    {
        return std::nullopt;
    }
    else
    {
        const nx::kit::Json& jsonPoints = jsonFigure["points"];
        if (!jsonPoints.is_array())
            return std::nullopt;

        std::vector<nx::kit::Json> jsonPointsAsArray = jsonPoints.array_items();
        result.reserve(jsonPointsAsArray.size());
        for (const auto &jsonPoint: jsonPointsAsArray)
            result.emplace_back(jsonPoint);

        if (direction)
        {
            const nx::kit::Json& jsonDirection = jsonFigure["direction"];
            if (!jsonDirection.is_string())
                return std::nullopt;

            auto optionalDirection = fromServerString<Direction>(jsonDirection.string_value().c_str());
            if (!optionalDirection)
                return std::nullopt;
            else
                *direction = *optionalDirection;
        }
    }

    if (label)
    {
        const nx::kit::Json& jsonLabel = json["label"];
        if (jsonLabel.is_string())
            *label = jsonLabel.string_value();
    }

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

//-----------------------

std::string pluginPointsToServerString(
    const std::vector<PluginPoint>& points,
    const std::string* label /*= nullptr*/,
    const Direction* direction /*= nullptr*/)
{
    if (points.empty())
        return "null"s;

    nx::kit::Json::array jsonPoints;
    jsonPoints.reserve(points.size());
    for (const auto& point: points)
    {
        nx::kit::Json jPoint = point.toJson();
        jsonPoints.push_back(jPoint);
    }

    //nx::kit::Json::object jsonPoints;
    //jsonPoints.insert(std::pair("points"s, jsonPointArray));

    nx::kit::Json::object jsonFigure;
    jsonFigure.insert(std::pair("points"s, jsonPoints));

    if (direction)
    {
        nx::kit::Json jsonDirection(toServerString(*direction));
        jsonFigure.insert(std::pair("direction"s, jsonDirection));
    }

    nx::kit::Json::object jsonResult;
    jsonResult.insert(std::pair("figure", jsonFigure));

    if (label)
    {
        nx::kit::Json jsonLabel(*label);
        jsonResult.insert(std::pair("label"s, jsonLabel));

    }

    std::string result = nx::kit::Json(jsonResult).dump();

//    std::string rrr = R"({ "figure":{"color":"#b2ff59", "points" : [[0.39666666666666667, 0.5096296296296297], [0.74, 0.5777777777777777]]}, "name" : "", "showOnCamera" : true })";

    return result;

}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<UnnamedBoxFigure> UnnamedBoxFigure::fromServerString(const char* source)
{
    std::optional<std::vector<PluginPoint>> boxPoints = ServerStringToPluginPoints(source);

    if (!boxPoints)
        return std::nullopt; //< Failed to read points.

    if (boxPoints->size() != 2)
        return std::nullopt; //< Has a wrong number of points.

    UnnamedBoxFigure result;
    result.width = std::abs((*boxPoints)[0].x - (*boxPoints)[1].x);
    result.height = std::abs((*boxPoints)[0].y - (*boxPoints)[1].y);
    return result;
}

std::string UnnamedBoxFigure::toServerString() const
{
    NX_ASSERT(width <= 1.0 && height <= 1.0);
    std::vector<PluginPoint> points;

    const double x0 = (1.0 - width) / 2.0;
    const double x1 = x0 + width;

    const double y0 = (1.0 - height) / 2.0;
    const double y1 = y0 + height;

    points.emplace_back(x0, y0);
    points.emplace_back(x1, y1);

    return pluginPointsToServerString(points);
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<UnnamedPolygon> UnnamedPolygon::fromServerString(const char* source)
{
    std::optional<std::vector<PluginPoint>> points = ServerStringToPluginPoints(source);
    if (!points)
        return std::nullopt; //< Failed to read points

    if (points->size() > 0 && points->size() < 4)
        return std::nullopt; //< Has some but not enough points

    UnnamedPolygon result;
    result.points = *points;
    return result;
}

std::string UnnamedPolygon::toServerString() const
{
    return pluginPointsToServerString(points);
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<NamedPolygon> NamedPolygon::fromServerString(const char* source)
{
    std::string name;
    std::optional<std::vector<PluginPoint>> points = ServerStringToPluginPoints(source, &name);

    if (!points)
        return std::nullopt; //< Failed to read points.

    if (points->size() > 0 && points->size() < 4)
        return std::nullopt; //< Has some but not enough points.

    NamedPolygon result;
    result.name = name;
    result.points = *points;
    return result;
}

std::string NamedPolygon::toServerString() const
{
    return pluginPointsToServerString(points, &name);
}

//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<NamedLineFigure> NamedLineFigure::fromServerString(const char* source)
{
    std::string name;
    Direction direction;
    std::optional<std::vector<PluginPoint>> points = ServerStringToPluginPoints(source, &name, &direction);

    if (!points)
        return std::nullopt; //< Failed to read points.

    if (points->size() != 0 && points->size() != 2)
        return std::nullopt; //< A wrong number of points is read.

    NamedLineFigure result;
    result.points = *points;
    result.direction = direction;
    result.name = name;
    return result;
}

std::string NamedLineFigure::toServerString() const
{
    return pluginPointsToServerString(points, &name, &direction);
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
