#pragma once

namespace nx::vms::client::desktop {

// Please note: enum is numeric-serialized, reordering is forbidden.
enum class RadassMode
{
    Auto = 0,
    High = 1,
    Low = 2,
    Custom = 3,
};

} // namespace nx::vms::client::desktop
