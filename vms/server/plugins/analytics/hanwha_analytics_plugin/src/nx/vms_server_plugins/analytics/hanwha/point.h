#pragma once

#include <vector>
#include <string_view>
#include <optional>

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

struct parser_error {};

/** Video frame dimensions - needed for translating relative coordinates to absolute. */
struct FrameSize
{
    int width = 1;
    int height = 1;
};

//-------------------------------------------------------------------------------------------------
/** Point in relative coordinates (plugin format). Every figure is a vector of such points.*/
struct PluginPoint
{
    double x = 0;
    double y = 0;
    PluginPoint() = default;
    PluginPoint(const nx::kit::Json& json);
    bool operator==(PluginPoint rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(PluginPoint rhs) const { return !(*this == rhs); }
    std::ostream& toSunapiStream(std::ostream& os, FrameSize frameSize) const;
};

/**
 * Parse the json string (received from server) that contains points. The points have two
 * coordinates of type double.
 * \return vector of points (may be empty). nullopt - if json is broken.
 */
std::optional<std::vector<PluginPoint>> parsePluginPoints(const char* value);

/**
 * Convert the points into a string in sunapi format (comma separated coordinates). The coordinates
 * are translated from relative values [0 .. 1) into absolute values [0 .. frame size].
 */
std::string pluginPointsToSunapiString(const std::vector<PluginPoint>& points, FrameSize frameSize);

//-------------------------------------------------------------------------------------------------

/** Point in absolute coordinates (SUNAPI format). Every figure is a vector of such points.*/
struct SunapiPoint
{
    int x = 0;
    int y = 0;
    static constexpr int memberCount = 2;
    SunapiPoint() = default;
    SunapiPoint(int nx, int ny) : x(nx), y(ny) {}
    bool operator==(SunapiPoint rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(SunapiPoint rhs) const { return !(*this == rhs); }
};

std::vector<SunapiPoint> sunapiStringToSunapiPoints(const std::string& points);

} // namespace nx::vms_server_plugins::analytics::hanwha
