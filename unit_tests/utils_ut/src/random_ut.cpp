#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace utils {
namespace random {
namespace test {

template<typename Type>
void testContainer(const std::map<Type, size_t>& container)
{
    // OK
}

template<typename Type, typename Expect, typename ... Expects>
void testContainer(const std::map<Type, size_t>& container, Expect expect, Expects ... args)
{
    const auto it = std::find_if(
        container.begin(), container.end(),
        [&](const std::pair<Type, size_t>& kv) { return expect(kv.first); });

    ASSERT_TRUE(it != container.end());
    testContainer(container, std::forward<Expects>(args) ...);
}

template<template<typename> class Dist, typename Type, typename ... Expects>
void testQtDevice(
    Type min, Type max, size_t number, Expects ... expects)
{
    Dist<Type> distribution(min, max);
    std::map<Type, size_t> container;
    while (number--)
        ++container[distribution(qtDevice())];

    testContainer(container, std::forward<Expects>(expects) ...);
}

TEST(QtDeviceTest, generation)
{
    testQtDevice<std::uniform_int_distribution, int>(
        0, 1, 100,
        [](int a) { return a == 0; },
        [](int a) { return a == 1; });

    testQtDevice<std::uniform_int_distribution, size_t>(
        0, 100, 10000,
        [](size_t a) { return a < 80; },
        [](size_t a) { return a > 40 && a < 60; },
        [](size_t a) { return a > 80; });

    testQtDevice<std::uniform_int_distribution, int>(
        -10, 10, 1000,
        [](int a) { return a > 5; },
        [](int a) { return a < -5; });

    testQtDevice<std::uniform_real_distribution, float>(
        -1, 1, 1000,
        [](float a) { return a > 0.9; },
        [](float a) { return a < -0.9; });

    testQtDevice<std::uniform_real_distribution, double>(
        0, 1, 100,
        [](double a) { return a > 0.8; },
        [](double a) { return a < 0.2; });
}

} // namespace test
} // namespace random
} // namespace utils
} // namespace nx
