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
    return (Sign) ((NumberType(0) - number) < (number < NumberType(0)));
}

} // namespace math
} // namespace utils
} // namespace nx
