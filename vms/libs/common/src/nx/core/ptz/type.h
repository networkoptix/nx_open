#pragma once

#include <array>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace ptz {

enum class Type
{
    none = 0,
    operational = 1 << 0,
    configurational = 1 << 1,
    any = operational | configurational,
};
Q_DECLARE_FLAGS(Types, Type);
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Type);

static const std::array<Type, 2> kAllTypes = {Type::operational, Type::configurational};

} // namespace ptz
} // namespace core
} // namespace nx

Q_DECLARE_OPERATORS_FOR_FLAGS(nx::core::ptz::Types)
QN_FUSION_DECLARE_FUNCTIONS(nx::core::ptz::Type, (metatype)(numeric)(lexical));
QN_FUSION_DECLARE_FUNCTIONS(nx::core::ptz::Types, (metatype)(numeric)(lexical));
