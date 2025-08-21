// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <nx/utils/json/flags.h>
#include <nx/vms/api/data/device_ptz_model.h>

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

inline std::optional<::nx::vms::api::MinMaxLimit> findComponent(
    const nx::vms::api::PtzPositionLimits& limits, Component c) noexcept
{
    using enum Component;
    switch (c)
    {
        case pan:
            return limits.pan;
        case tilt:
            return limits.tilt;
        case rotation:
            return limits.rotation;
        case zoom:
            return limits.fov;
        case focus:
            return limits.focus;
        default:
            return std::nullopt;
    }
}

} // namespace ptz
} // namespace nx::vms::common
