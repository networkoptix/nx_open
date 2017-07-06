#pragma once

#include <map>
#include <QtCore/QString>

namespace nx {
namespace utils {
namespace log {

enum class Level
{
    undefined,
    none,
    always,
    error,
    warning,
    info,
    debug,
    verbose
};

bool NX_UTILS_API operator<=(Level left, Level right);
Level NX_UTILS_API levelFromString(const QString& levelString);
QString NX_UTILS_API toString(Level level);

using LevelFilters = std::map<QString, Level>;

/**
 * Parses filters in format (default level is verbose:
 *     filterX1[,filterX2[,...]][-levelX][,filterY1[;filterY2,...][-levelY]][;...]
 *
 * Examples:
 *     A-debug;B;C-info -> A: debug, B: verbose; C: info
 *     x,y-info;a,b -> x: info, y: info, a: verbose, b: verbose
 */
LevelFilters NX_UTILS_API levelFiltersFromString(const QString& levelFilters);

} // namespace log
} // namespace utils
} // namespace nx
