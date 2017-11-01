#pragma once

#include <random>
#include <limits>
#include <type_traits>
#include <QByteArray>

#include "qt_random_device.h"

// #define NX_UTILS_USE_OWN_INT_DISTRIBUTION

namespace nx {
namespace utils {
namespace random {

/**
 * Simple implementaion of std::uniform_int_distribution.
 * @note Currenty this distribution is not uniform, because of overflow bug.
 *     Do not use it without a dare need.
 */
template<typename Type = int>
class UniformIntDistribution
{
public:
    UniformIntDistribution(Type min, Type max):
        m_min(min),
        m_range(max - min)
    {
    }

    template<typename Device>
    Type operator()(Device& device) const
    {
        // NOTE: uint64_t is used becauce it is likely biggest type avalible.
        static const uint64_t deviceRange = (uint64_t) (Device::max() - Device::min());
        const auto makeNumber = [&device]() { return (Type) (device() - Device::min()); };

        Type number = makeNumber();
        uint64_t range = deviceRange;
        while (range < (uint64_t) m_range)
        {
            number = number * deviceRange + makeNumber();

            const auto oldRange = range;
            range *= deviceRange;
            if (range / deviceRange != oldRange)
                break; // Range overflow, we got enough.
        }

        if (std::numeric_limits<Type>::max() == m_range)
            return m_min + number; //< Awoid range owerflow.

        return m_min + (number % (m_range + 1));
    }

private:
    const Type m_min;
    const Type m_range;
};

template<>
class UniformIntDistribution<char>
{
    // Char specialization is not defined by standart, as it is not even clear if it is signed.
};

/** Thread local QtDevice. */
NX_UTILS_API QtDevice& qtDevice();

/**
 * Generates uniform_int_distribution random data the length of count.
 */
NX_UTILS_API QByteArray generate(std::size_t count);

/**
 * Generates uniform_int_distribution random integer in [min, max]
 */
template<typename RandomDevice, typename Type = int>
Type number(
    RandomDevice& randomDevice,
    Type min = 0,
    Type max = std::numeric_limits<Type>::max(),
    typename std::enable_if<std::is_class<RandomDevice>::value>::type* = 0,
    typename std::enable_if<
        std::is_integral<Type>::value &&
        !std::is_same<Type, bool>::value>::type* = 0)
{
    #ifdef NX_UTILS_USE_OWN_INT_DISTRIBUTION
        UniformIntDistribution<Type> distribution(min, max);
    #else
        std::uniform_int_distribution<Type> distribution(min, max);
    #endif
    return distribution(randomDevice);
}

template<typename Type = int>
Type number(
    Type min = 0,
    Type max = std::numeric_limits<Type>::max(),
    typename std::enable_if<
        std::is_integral<Type>::value &&
        !std::is_same<Type, bool>::value>::type* = 0)
{
    return number<QtDevice, Type>(qtDevice(), min, max);
}

template<typename RandomDevice, typename Type>
bool number(
    RandomDevice& randomDevice,
    typename std::enable_if<std::is_class<RandomDevice>::value>::type* = 0,
    typename std::enable_if<std::is_same<Type, bool>::value>::type* = 0)
{
    return randomDevice() >
        (RandomDevice::min() + ((RandomDevice::max() - RandomDevice::min()) / 2));
}

template<typename Type>
bool number(typename std::enable_if<std::is_same<Type, bool>::value>::type* = 0)
{
    return number<QtDevice, Type>(qtDevice());
}

template<typename RandomDevice, typename Type>
Type number(
    RandomDevice& randomDevice,
    Type min = 0,
    Type max = std::numeric_limits<Type>::max(),
    typename std::enable_if<std::is_class<RandomDevice>::value>::type* = 0,
    typename std::enable_if<std::is_floating_point<Type>::value>::type* = 0)
{
    std::uniform_real_distribution<Type> distribution(min, max);
    return distribution(randomDevice);
}

/**
 * Generates uniform_real_distribution random real in [min, max)
 */
template<typename Type>
Type number(
    Type min = 0,
    Type max = std::numeric_limits<Type>::max(),
    typename std::enable_if<std::is_floating_point<Type>::value>::type* = 0)
{
    return number<QtDevice, Type>(qtDevice(), min, max);
}

template<typename RandomDevice>
QByteArray generateName(RandomDevice& randomDevice, int length)
{
    static const char kAlphaAndDigits[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static const size_t kDigitsCount = 10;
    static_assert(kDigitsCount < sizeof(kAlphaAndDigits), "Check kAlphaAndDigits array");

    if (!length)
        return QByteArray();

    QByteArray str;
    str.resize(length);
    str[0] = kAlphaAndDigits[
        nx::utils::random::number(randomDevice) % 
            (sizeof(kAlphaAndDigits) / sizeof(*kAlphaAndDigits) - kDigitsCount - 1)];
    for (int i = 1; i < length; ++i)
    {
        str[i] = kAlphaAndDigits[nx::utils::random::number(randomDevice) % 
            (sizeof(kAlphaAndDigits) / sizeof(*kAlphaAndDigits) - 1)];
    }

    return str;
}

NX_UTILS_API QByteArray generateName(int length);

/**
 * Generates uniform random number in [base - delta, base + delta]
 */
template<typename Type>
Type numberDelta(Type base, Type delta) { return number(base - delta, base + delta); }

/**
 * Returns ref on random element in container.
 */
template<typename Container>
typename Container::value_type& choice(Container& container)
{
    auto position = number<typename Container::size_type>(0, container.size() - 1);
    return *std::next(container.begin(), position);
}

template<typename Container>
const typename Container::value_type& choice(const Container& container)
{
    auto position = number<typename Container::size_type>(0, container.size() - 1);
    return *std::next(container.begin(), position);
}

} // namespace random
} // namespace utils
} // namespace nx
