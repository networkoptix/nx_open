#pragma once

#include <random>
#include <limits>
#include <type_traits>
#include <QByteArray>

namespace nx {
namespace utils {
namespace random {

/**
 * Exception free, qrand based random device.
 * @note Should be used on platforms where std::random_device does not work as expected.
 */
class NX_UTILS_API QtDevice
{
public:
    typedef int result_type;
    QtDevice();

    result_type operator()();
    double entropy() const;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return RAND_MAX; }
};

/** Thread local QtDevice. */
NX_UTILS_API QtDevice& qtDevice();

/**
 * Generates uniform_int_distribution random data the length of count.
 */
NX_UTILS_API QByteArray generate(
    std::size_t count,
    char min = std::numeric_limits<char>::min(),
    char max = std::numeric_limits<char>::max());

/**
 * Generates uniform_int_distribution random integer in [min, max]
 */
template<typename Type = int>
Type number(
    Type min = 0,
    Type max = std::numeric_limits<Type>::max(),
    typename std::enable_if<std::is_integral<Type>::value>::type* = 0)
{
    if (max == min)
        return min;

    return min + (qtDevice()() % (max - min));

    //std::uniform_int_distribution<Type> distribution(min, max);
    //return distribution(qtDevice());
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
    std::uniform_real_distribution<Type> distribution(min, max);
    return distribution(qtDevice());
}

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
