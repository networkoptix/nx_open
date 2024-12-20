// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <nx/utils/json/flags.h>

namespace nx::vms::common {
namespace ptz {

NX_REFLECTION_ENUM_CLASS(Component,
    pan = 1 << 0,
    tilt = 1 << 1,
    rotation = 1 << 2,
    zoom = 1 << 3,
    focus = 1 << 4
)

Q_DECLARE_FLAGS(Components, Component)
Q_DECLARE_OPERATORS_FOR_FLAGS(Components)

template<int VectorSize>
using ComponentVector = std::array<Component, VectorSize>;

static const ComponentVector<5> kAllComponents = {
    Component::pan,
    Component::tilt,
    Component::rotation,
    Component::zoom,
    Component::focus
};

} // namespace ptz
} // namespace nx::vms::common
