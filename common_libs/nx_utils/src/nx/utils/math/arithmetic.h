#pragma once

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

} // namespace math
} // namespace utils
} // namespace nx
