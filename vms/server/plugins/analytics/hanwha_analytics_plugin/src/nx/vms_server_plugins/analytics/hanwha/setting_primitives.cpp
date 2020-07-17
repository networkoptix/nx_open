#include "setting_primitives.h"

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

std::optional<Direction> directionFromServerString(const char* source)
{
    std::optional<Direction> result;
    if (!source)
        return result;

    if (strcmp(source, "right") == 0)
        result = Direction::Right;

    else if (strcmp(source, "left") == 0)
        result = Direction::Left;

    else if (strcmp(source, "absent") == 0)
        result = Direction::Both;

    return result;
}

//-------------------------------------------------------------------------------------------------

std::string directionToServerString(Direction direction)
{
    switch (direction)
    {
        case Direction::Right: return "right";
        case Direction::Left: return "left";
        case Direction::Both: return "absent";
    }
    //NX_ASSERT(false, "unexpected Direction value");
    return "";
}

//-------------------------------------------------------------------------------------------------

/**
 * Parse the json string (received from server) that contains points. The points have two
 * coordinates of type double.
 * \return vector of points (may be empty). nullopt - if json is broken.
 */
std::optional<std::vector<PluginPoint>> ServerStringToPluginPoints(
    const char* source,
    std::string* label = nullptr,
    Direction* direction = nullptr)
{
/**
The source example:
{"figure":{"color":"#e040fb","direction":"right","points":[[0.3567708432674408,0.5513888597488403],[0.3565104305744171,0.8416666388511658]]},"label":"","showOnCamera":true}
*/
    NX_ASSERT(source);
    std::vector<PluginPoint> result;

    if (strcmp(source, "null") == 0)
        return std::nullopt; //< Inadmissible value.

    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(source, err);
    if (!json.is_object())
        return std::nullopt;

    const nx::kit::Json& jsonFigure = json["figure"];
    if (jsonFigure.is_null())
    {
        if (label)
            *label = std::string();
        if (direction)
            *direction = Direction();
        return result; //< Empty figure.
    }
    else if (!jsonFigure.is_object())
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

            auto optionalDirection = directionFromServerString(jsonDirection.string_value().c_str());
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

std::string pluginPointsToServerString(
    const std::vector<PluginPoint>& points,
    const std::string* label = nullptr,
    const Direction* direction = nullptr)
{
    if (points.empty())
        return "{}"s;

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
        nx::kit::Json jsonDirection(directionToServerString(*direction));
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
/**
The result example:
{"figure": {"direction": "right", "points": [[0.3567708432674408, 0.55138885974884033], [0.35651043057441711, 0.84166663885116577]]}, "label": ""}
*/
    return result;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

/*static*/ std::optional<ObjectSizeConstraints> ObjectSizeConstraints::fromServerString(
    const char* source)
{
    ObjectSizeConstraints result;

    std::string err;
    nx::kit::Json json = nx::kit::Json::parse(source, err);
    if (!json.is_object())
        return std::nullopt;

    const nx::kit::Json& jsonMinimum = json["minimum"];
    if (!jsonMinimum.is_array())
        return std::nullopt;
    std::vector<nx::kit::Json> jsonMinimumAsArray = jsonMinimum.array_items();
    if (jsonMinimumAsArray.size() != 2)
        return std::nullopt;
    result.minWidth = jsonMinimumAsArray[0].number_value();
    result.minHeight = jsonMinimumAsArray[1].number_value();

    const nx::kit::Json& jsonMaximum = json["maximum"];
    if (!jsonMaximum.is_array())
        return std::nullopt;
    std::vector<nx::kit::Json> jsonMaximumAsArray = jsonMaximum.array_items();
    if (jsonMaximumAsArray.size() != 2)
        return std::nullopt;
    result.maxWidth = jsonMaximumAsArray[0].number_value();
    result.maxHeight = jsonMaximumAsArray[1].number_value();
    return result;
}

std::string ObjectSizeConstraints::toServerString() const
{
    // The example of the desired result:
    // {
    //    "minimum": [0.2, 0.3],
    //    "maximum": [0.8, 0.9],
    //    "positions":
    //    [
    //        [0.4, 0.35],
    //        [0.1, 0.05]
    //    ]
    // }

    auto offset = [](double size) { return (1.0 - size) / 2; };

    nx::kit::Json::array minSizeArray{ double(minWidth), double(minHeight) };
    nx::kit::Json::array maxSizeArray{ double(maxWidth), double(maxHeight) };

    nx::kit::Json::object jsonResult;
    jsonResult.insert(std::pair("minimum", minSizeArray));
    jsonResult.insert(std::pair("maximum", maxSizeArray));

    nx::kit::Json::array minOffsetArray{ 0, 0 };// offset(minWidth), offset(minHeight) };
    nx::kit::Json::array maxOffsetArray{ 0, 0 };// offset(maxWidth), offset(maxHeight) };

    nx::kit::Json::array offsetArray{ minOffsetArray, maxOffsetArray };
//    jsonResult.insert(std::pair("positions", offsetArray));

    std::string result = nx::kit::Json(jsonResult).dump();
    return result;
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
