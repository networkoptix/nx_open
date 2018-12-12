#pragma once

#include <cmath>
#include <type_traits>
#include <nx/utils/std/optional.h>

namespace nx {
namespace utils {
namespace math {

enum class Sign
{
    negative = -1,
    zero = 0,
    positive = 1,
};

template <typename NumberType>
inline Sign sign(NumberType number)
{
    if (number < 0)
        return Sign::negative;
    else if (number > 0)
        return Sign::positive;

    return Sign::zero;
}

template<typename Number>
constexpr std::optional<Number> remainder(Number dividend, Number divider)
{
    if (divider == 0)
        return std::nullopt;

    if constexpr (std::is_integral<Number>::value)
        return dividend % divider;

    if constexpr (std::is_floating_point<Number>::value)
        return fmod(dividend, divider);

    return std::nullopt;
}

} // namespace math
} // namespace utils
} // namespace nx
