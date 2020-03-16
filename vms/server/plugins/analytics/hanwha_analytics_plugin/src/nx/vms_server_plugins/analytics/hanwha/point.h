#pragma once

#include <vector>
#include <string_view>
#include <optional>
#include <algorithm>
#include <cstring>

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

struct parser_error {};

/** Video frame dimensions - needed for translating relative coordinates to absolute. */
struct FrameSize
{
    int width = 1;
    int height = 1;
    FrameSize() = default;
    FrameSize(int width, int height) : width(width), height(height) {}

    bool operator<(const FrameSize other) const
    {
        return width * height < other.width*other.height;
    }

    int xRelativeToAbsolute(double x) const
    {
        int result = int(x * width + 0.5);
        return std::min(result, width - 1);
    };
    int yRelativeToAbsolute(double y) const
    {
        int result = int(y * height + 0.5);
        return std::min(result, height - 1);
    };

    double xAbsoluteToRelative(int x) const
    {
        return float(x) / width;
    };

    double yAbsoluteToRelative(int y) const
    {
        return float(y) / height;
    };
};

//-------------------------------------------------------------------------------------------------
/** Point in relative coordinates (plugin format). Every figure is a vector of such points.*/
struct PluginPoint
{
    double x = 0;
    double y = 0;
    PluginPoint() = default;
    PluginPoint(double nx, double ny): x(nx), y(ny) {}
    PluginPoint(const nx::kit::Json& json);
    bool operator==(PluginPoint rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(PluginPoint rhs) const { return !(*this == rhs); }

    nx::kit::Json toJson() const;

    std::ostream& toSunapiStream(std::ostream& os, FrameSize frameSize) const;

    bool fromSunapiString(const std::string& value, FrameSize frameSize);
    bool fromSunapiString(const char*& begin, const char* end, FrameSize frameSize);
    std::istream& fromSunapiStream(std::istream& is, FrameSize frameSize);
};

// We need separate typed for width and height
struct Width
{
    double value = 0.;
    Width& operator=(double v) { value = v; return *this; }
    operator double() const { return value; }
};

struct Height
{
    double value = 0.;
    Height& operator=(double v) { value = v; return *this; }
    operator double() const { return value; }
};

std::optional<std::vector<PluginPoint>> WidthHeightToPluginPoints(Width width, Height height);

enum class Direction { Right, Left, Both };

template<typename SettingType>
std::optional<SettingType> fromServerString(const char* source);

template<>
inline std::optional<Direction> fromServerString<Direction>(const char* source)
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

    /*
     GUI Team has not implemented direction support yet, so we consider all directions to be
     bidirectional. This should be fixed later.
    */
    result = Direction::Both;
    return result;
}

inline std::string toServerString(Direction direction)
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

struct NamedLineFigure
{
    std::vector<PluginPoint> points;
    Direction direction = Direction();
    std::string name;

    bool operator==(const NamedLineFigure& rhs) const
        { return points == rhs.points && direction == rhs.direction && name == rhs.name; }

    static std::optional<NamedLineFigure> fromServerString(const char* source);
    std::string toServerString() const;
};

struct UnnamedBoxFigure
{
    Width width;
    Height height;
    static std::optional<UnnamedBoxFigure> fromServerString(const char* source);
    std::string toServerString() const;
};

struct ObjectSizeConstraints
{
    Width minWidth;
    Height minHeight;
    Width maxWidth;
    Height maxHeight;
    static std::optional<ObjectSizeConstraints> fromServerString(const char* source);
    std::string toServerString() const;
};

struct UnnamedPolygon
{
    std::vector<PluginPoint> points;

    bool operator==(const UnnamedPolygon& rhs) const { return points == rhs.points; }
    static std::optional<UnnamedPolygon> fromServerString(const char* source);
    std::string toServerString() const;
};

struct NamedPolygon
{
    std::vector<PluginPoint> points;
    std::string name;

    bool operator==(const NamedPolygon& rhs) const { return points == rhs.points && name == rhs.name; }
    static std::optional<NamedPolygon> fromServerString(const char* source);
    std::string toServerString() const;
};

/**
 * Parse the json string (received from server) that contains points. The points have two
 * coordinates of type double.
 * \return vector of points (may be empty). nullopt - if json is broken.
 */
std::optional<std::vector<PluginPoint>> ServerStringToPluginPoints(
    const char* source, std::string* label = nullptr, Direction* direction = nullptr);

std::optional<Width> ServerStringToWidth(const char* value);

std::optional<Height> ServerStringToHeight(const char* value);

std::optional<Direction> ServerStringToDirection(const char* value);

/**
 * Convert the points into a string in sunapi format (comma separated coordinates). The coordinates
 * are translated from relative values [0 .. 1) into absolute values [0 .. frame size].
 */
std::string pluginPointsToSunapiString(const std::vector<PluginPoint>& points, FrameSize frameSize);

std::string pluginPointsToServerString(const std::vector<PluginPoint>& points,
    const std::string* label = nullptr, const Direction* direction = nullptr);

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
