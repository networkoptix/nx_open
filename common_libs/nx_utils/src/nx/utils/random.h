#pragma once

#include <random>
#include <limits>
#include <type_traits>
#include <QByteArray>

namespace nx {
namespace utils {
namespace random {

/**
 * Thread local random device
 */
NX_UTILS_API std::random_device& device();

/**
 * Generates uniform_int_distribution random data the length of count.
 */
NX_UTILS_API QByteArray generate(std::size_t count, bool* isOk = nullptr);

/**
 * Generates uniform_int_distribution random integer in [min, max]
 */
template<typename Type>
Type number(
    Type min = std::numeric_limits<Type>::min(),
    Type max = std::numeric_limits<Type>::max(),
    bool* isOk = nullptr,
    typename std::enable_if<std::is_integral<Type>::value>::type* = 0)
{
    try
    {
        if (isOk)
            *isOk = true;

        std::uniform_int_distribution<Type> distribution(min, max);
        return distribution(device());
    }
    catch(const std::exception&)
    {
        if (isOk)
            *isOk = false;

        return (min + max) / 2; // TODO: is it ok?
    }
}

/**
 * Generates uniform_real_distribution random real in [min, max)
 */
template<typename Type>
Type number(
    Type min = std::numeric_limits<Type>::min(),
    Type max = std::numeric_limits<Type>::max(),
    bool* isOk = nullptr,
    typename std::enable_if<std::is_floating_point<Type>::value>::type* = 0)
{
    try
    {
        if (isOk)
            *isOk = true;

        std::uniform_real_distribution<Type> distribution(min, max);
        return distribution(device());
    }
    catch(const std::exception&)
    {
        if (isOk)
            *isOk = false;

        return (min + max) / 2;
    }
}

} // namespace random
} // namespace utils
} // namespace nx
