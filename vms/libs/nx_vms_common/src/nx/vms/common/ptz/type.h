// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

namespace nx::vms::common {
namespace ptz {

enum class Type
{
    /**%apidoc[unused] */
    none = 0,
    operational = 1 << 0,
    configurational = 1 << 1,

    /**%apidoc[unused] */
    any = operational | configurational,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(Type*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<Type>;
    return visitor(
        Item{Type::operational, "operational"},
        Item{Type::configurational, "configurational"}
    );
}

Q_DECLARE_FLAGS(Types, Type)
Q_DECLARE_OPERATORS_FOR_FLAGS(Types)

static const std::array<Type, 2> kAllTypes = {Type::operational, Type::configurational};

} // namespace ptz
} // namespace nx::vms::common
