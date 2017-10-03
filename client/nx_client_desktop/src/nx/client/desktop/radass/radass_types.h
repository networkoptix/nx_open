#pragma once

namespace nx {
namespace client {
namespace desktop {

// Please note: enum is numeric-serialized, reordering is forbidden.
enum class RadassMode
{
    Auto = 0,
    High = 1,
    Low = 2,
    Custom = 3,
};

} // namespace desktop
} // namespace client
} // namespace nx