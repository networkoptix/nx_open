#pragma once

#include <array>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace ptz {

enum class Component
{
    pan = 1 << 0,
    tilt = 1 << 1,
    rotation = 1 << 2,
    zoom = 1 << 3,
};
Q_DECLARE_FLAGS(Components, Component)
Q_DECLARE_OPERATORS_FOR_FLAGS(Components)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Component)

template<int VectorSize>
using ComponentVector = std::array<Component, VectorSize>;

} // namespace ptz
} // namespace core
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::core::ptz::Component)
    (nx::core::ptz::Components),
    (metatype)(numeric)(lexical))
