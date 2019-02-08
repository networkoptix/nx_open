#pragma once

namespace nx {
namespace core {
namespace access {

/** Types of base providers, sorted by priority. */
enum class Source
{
    none,
    permissions,    //< Accessible by permissions.
    shared,         //< Accessible by direct sharing.
    layout,         //< Accessible by placing on shared layout.
    videowall,      //< Accessible by placing on videowall.
};

inline uint qHash(Source value, uint seed)
{
    return ::qHash(static_cast<int>(value), seed);
}

enum class Mode
{
    cached,
    direct
};

} // namespace access
} // namespace core
} // namespace nx