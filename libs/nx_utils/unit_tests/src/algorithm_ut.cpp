// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {
namespace test {

class AlgorithmMoveIf:
    public ::testing::Test
{
protected:
    void performFixedDataTest(
        const std::vector<int>& initialArray,
        const int splitValue,
        const std::vector<int>& initialArrayAfterTest,
        const std::vector<int>& resultingArrayAfterTest)
    {
        givenInitialArray(initialArray);
        whenMoveValuesGreaterThan(splitValue);
        thenInitialArrayShouldBeEqualTo(initialArrayAfterTest);
        thenResultingArrayShouldBeEqualTo(resultingArrayAfterTest);
    }

    void givenInitialArray(const std::vector<int>& initialArrayValues)
    {
        for (const int val: initialArrayValues)
            m_initialData.push_back(std::make_unique<int>(val));
    }

    void whenMoveValuesGreaterThan(int splitValue)
    {
        auto newEnd = nx::utils::move_if(
            m_initialData.begin(), m_initialData.end(), std::back_inserter(m_resultingData),
            [splitValue](const std::unique_ptr<int>& elem)
            {
                return *elem > splitValue;
            });
        m_initialData.erase(newEnd, m_initialData.end());
    }

    void thenInitialArrayShouldBeEqualTo(const std::vector<int>& initialArrayValuesAfterTest)
    {
        assertEquals(initialArrayValuesAfterTest, m_initialData);
    }

    void thenResultingArrayShouldBeEqualTo(const std::vector<int>& resultingArrayValuesAfterTest)
    {
        assertEquals(resultingArrayValuesAfterTest, m_resultingData);
    }

private:
    std::vector<std::unique_ptr<int>> m_initialData;
    std::vector<std::unique_ptr<int>> m_resultingData;

    void assertEquals(
        const std::vector<int>& expected,
        const std::vector<std::unique_ptr<int>>& actual)
    {
        ASSERT_EQ(expected.size(), actual.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
            ASSERT_EQ(expected[i], *(actual[i]));
    }
};

TEST_F(AlgorithmMoveIf, actually_splits_array)
{
    const std::vector<int> initialArray = { 1, 2, 8, 5, 3, 7, 4, 9 };
    const int splitValue = 5;
    const std::vector<int> initialArrayAfterTest = { 1, 2, 5, 3, 4 };
    const std::vector<int> resultingArrayAfterTest = { 8, 7, 9 };

    performFixedDataTest(initialArray, splitValue, initialArrayAfterTest, resultingArrayAfterTest);
}

TEST_F(AlgorithmMoveIf, whole_initial_data_is_moved)
{
    const std::vector<int> initialArray = { 1, 2, 8, 5, 3, 7, 4, 9 };
    const int splitValue = 0;
    const std::vector<int> initialArrayAfterTest = {};
    const std::vector<int> resultingArrayAfterTest = { 1, 2, 8, 5, 3, 7, 4, 9 };

    performFixedDataTest(initialArray, splitValue, initialArrayAfterTest, resultingArrayAfterTest);
}

TEST_F(AlgorithmMoveIf, whole_initial_data_is_left_in_place)
{
    const std::vector<int> initialArray = { 1, 2, 8, 5, 3, 7, 4, 9 };
    const int splitValue = 10;
    const std::vector<int> initialArrayAfterTest = { 1, 2, 8, 5, 3, 7, 4, 9 };
    const std::vector<int> resultingArrayAfterTest = {};

    performFixedDataTest(
        initialArray,
        splitValue,
        initialArrayAfterTest,
        resultingArrayAfterTest);
}

TEST(Algorithm_y_combinator, compilation_test)
{
    auto addItemRecursive = nx::utils::y_combinator(
        [](auto addItemRecursive, int a) -> int
        {
            if (a == 10)
                return a;

            return addItemRecursive(a + 1);
        });

    ASSERT_EQ(addItemRecursive(0), 10);
}

} // namespace test
} // namespace utils
} // namespace nx
