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
    static_cast<void>(container);
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

template<typename Type, typename ... Expects>
void testNumberGeneration(
    Type min, Type max, size_t count, Expects ... expects)
{
    std::map<Type, size_t> container;
    while (count--)
        ++container[number(min, max)];

    testContainer(container, std::forward<Expects>(expects) ...);
}

TEST(Random, Numbers)
{
    testNumberGeneration<int>(
        0, 1, 100,
        [](int a) { return a == 0; },
        [](int a) { return a == 1; });

    testNumberGeneration<size_t>(
        0, 100, 10000,
        [](size_t a) { return a < 80; },
        [](size_t a) { return a > 40 && a < 60; },
        [](size_t a) { return a > 80; });

    testNumberGeneration<int>(
        -10, 10, 1000,
        [](int a) { return a > 5; },
        [](int a) { return a < -5; });

    testNumberGeneration<float>(
        -1, 1, 1000,
        [](float a) { return a > 0.9; },
        [](float a) { return a < -0.9; });

    testNumberGeneration<double>(
        0, 1, 100,
        [](double a) { return a > 0.8; },
        [](double a) { return a < 0.2; });

    testNumberGeneration<uint64_t>(
        0, std::numeric_limits<uint64_t>::max(), 100,
        [](uint64_t a) { return a > std::numeric_limits<uint64_t>::max() / 2; },
        [](uint64_t a) { return a < std::numeric_limits<uint64_t>::max() / 2; });
}

} // namespace test
} // namespace random
} // namespace utils
} // namespace nx
