// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <initializer_list>
#include <limits>
#include <random>
#include <string_view>
#include <type_traits>

#include <QtCore/QByteArray>
#include <QtCore/QRandomGenerator>

#include <nx/concepts/common.h>

// #define NX_UTILS_USE_OWN_INT_DISTRIBUTION

namespace nx::utils::random {

/**
 * Simple implementation of std::uniform_int_distribution.
 * NOTE: Currently this distribution is not uniform, because of overflow bug.
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
        // NOTE: uint64_t is used because it is likely biggest type available.
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
            return m_min + number; //< Avoid range overflow.

        return m_min + (number % (m_range + 1));
    }

private:
    const Type m_min;
    const Type m_range;
};

template<>
class UniformIntDistribution<char>
{
    // Char specialization is not defined by standard, as it is not even clear if it is signed.
};

inline QRandomGenerator& defaultGenerator() { return *QRandomGenerator::global(); }

template<typename StringType = QByteArray>
// requires String<StringType>
StringType generate(int count)
{
    auto& device = defaultGenerator();

    // NOTE: Direct device access with simple cast works significantly faster than
    //     uniform_int_distribution on a number of platforms (STL implementations).
    //     Found on iOS by profiler.

    StringType data(count, 0);
    std::generate(
        data.begin(), data.end(),
        [&device]() { return (char) device(); });

    return data;
}

/**
 * Generates uniform_int_distribution random integer in [min, max].
 */
template<NonBoolIntegral Type, typename Generator>
requires std::is_class_v<Generator>
Type number(Generator& generator, Type min, Type max)
{
    #ifdef NX_UTILS_USE_OWN_INT_DISTRIBUTION
        UniformIntDistribution<Type> distribution(min, max);
    #else
        std::uniform_int_distribution<Type> distribution(min, max);
    #endif
    return distribution(generator);
}

template<NonBoolIntegral Type>
Type number(Type min, Type max)
{
    return number(defaultGenerator(), min, max);
}

template<NonBoolIntegral Type = int>
Type number()
{
    return number<Type>(0, std::numeric_limits<Type>::max());
}

/**
 * Generates uniform_real_distribution random real in [min, max)
 */
template<std::floating_point Type, typename Generator>
requires std::is_class_v<Generator>
Type number(Generator& generator, Type min, Type max)
{
    std::uniform_real_distribution<Type> distribution(min, max);
    return distribution(generator);
}

template<std::floating_point Type>
Type number(Type min, Type max)
{
    return number(defaultGenerator(), min, max);
}

/**
 * Generates uniform random number in [base - delta, base + delta]
 */
template<typename Type>
Type numberDelta(Type base, Type delta) { return number(base - delta, base + delta); }

/** Generates random boolean value. */
template<typename Generator>
requires std::is_class_v<Generator>
bool coin(Generator& generator)
{
    std::bernoulli_distribution bd(0.5);

    return bd(generator);
}

inline bool coin()
{
    return coin(defaultGenerator());
}

/** Generates random alfanumeric name starting with a letter. */
template<typename String = std::string, typename Generator>
requires std::is_class_v<Generator>
String generateName(Generator& generator, int length)
{
    static constexpr std::string_view kAlphaAndDigits =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static constexpr size_t kDigitsCount = 10;
    static_assert(kDigitsCount < kAlphaAndDigits.size(), "Check kAlphaAndDigits array");

    if (!length)
        return String();

    String str;
    str.resize(length);
    str[0] = kAlphaAndDigits[
        number(generator, (size_t) 0, kAlphaAndDigits.size() - kDigitsCount - 1)];
    for (int i = 1; i < length; ++i)
    {
        str[i] = kAlphaAndDigits[number(generator, (size_t) 0, kAlphaAndDigits.size() - 1)];
    }

    return str;
}

template<typename String = std::string>
String generateName(int length)
{
    return generateName<String>(defaultGenerator(), length);
}

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

template<typename T>
T choice(std::initializer_list<T> values)
{
    auto position = number<typename std::initializer_list<T>::size_type>(0, values.size() - 1);
    return *std::next(values.begin(), position);
}

template<typename Iterator>
typename Iterator::value_type& choice(Iterator begin, Iterator end)
{
    auto position = number<std::size_t>(0, std::distance(begin, end) - 1);
    return *std::next(begin, position);
}

} // namespace nx::utils::random
