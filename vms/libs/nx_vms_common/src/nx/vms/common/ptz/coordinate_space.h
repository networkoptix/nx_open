// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::common {
namespace ptz {

enum class CoordinateSpace
{
    device,
    logical
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(CoordinateSpace*, Visitor&& visitor)
{
    // ATTENTION: Assigning backwards-compatible values.
    // Such values should go before the correct ones, so both versions are supported on input,
    // and only deprecated version is supported on output.
    using Item = nx::reflect::enumeration::Item<CoordinateSpace>;
    return visitor(
        Item{CoordinateSpace::device, "DevicePtzCoordinateSpace"},
        Item{CoordinateSpace::device, "device"},
        Item{CoordinateSpace::logical, "LogicalPtzCoordinateSpace"},
        Item{CoordinateSpace::logical, "logical"}
    );
}

} // namespace ptz
} // namespace nx::vms::common
