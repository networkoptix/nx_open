#pragma once

#include <vector>
#include <string_view>
#include <optional>
#include <algorithm>
#include <cstring>

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

/** Video frame dimensions - needed for translating relative coordinates to absolute. */
struct RoiResolution
{
    int rawWidth = 1;
    int rawHeight = 1;
    bool isRotated = false;
    RoiResolution() = default;
    RoiResolution(int width, int height) : rawWidth(width), rawHeight(height) {}

    int width() const { return isRotated ? rawHeight : rawWidth; }
    int height() const { return isRotated ? rawWidth : rawHeight; }

    int area() const { return rawWidth * rawHeight; }

    bool operator<(const RoiResolution other) const
    {
        return area() < other.area();
    }

    int xRelativeToAbsolute(double x) const
    {
        int result = int(x * width() + 0.5);
        return std::min(result, width() - 1);
    };
    int yRelativeToAbsolute(double y) const
    {
        int result = int(y * height() + 0.5);
        return std::min(result, height() - 1);
    };

    double xAbsoluteToRelative(int x) const
    {
        return float(x) / width();
    };

    double yAbsoluteToRelative(int y) const
    {
        return float(y) / height();
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

    std::ostream& toSunapiStream(std::ostream& os, RoiResolution roiResolution) const;

    bool fromSunapiString(const std::string& value, RoiResolution roiResolution);
    bool fromSunapiString(const char*& begin, const char* end, RoiResolution roiResolution);
    std::istream& fromSunapiStream(std::istream& is, RoiResolution roiResolution);
};

//-------------------------------------------------------------------------------------------------

enum class Direction { Right, Left, Both };

//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------

struct ObjectSizeConstraints
{
    double minWidth = 0.1;
    double minHeight = 0.1;
    double maxWidth = 0.9;
    double maxHeight = 0.9;
    static std::optional<ObjectSizeConstraints> fromServerString(const char* source);
    std::string toServerString() const;
};

//-------------------------------------------------------------------------------------------------

struct UnnamedPolygon
{
    std::vector<PluginPoint> points;

    bool operator==(const UnnamedPolygon& rhs) const { return points == rhs.points; }
    static std::optional<UnnamedPolygon> fromServerString(const char* source);
    std::string toServerString() const;
};

//-------------------------------------------------------------------------------------------------

struct NamedPolygon
{
    std::vector<PluginPoint> points;
    std::string name;

    bool operator==(const NamedPolygon& rhs) const { return points == rhs.points && name == rhs.name; }
    static std::optional<NamedPolygon> fromServerString(const char* source);
    std::string toServerString() const;
};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
