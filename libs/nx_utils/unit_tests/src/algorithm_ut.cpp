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

//-------------------------------------------------------------------------------------------------
// reverseWords

TEST(Algorithm_reverseWords, common)
{
    ASSERT_EQ("com.google.inbox", reverseWords<std::string>("inbox.google.com", "."));
    ASSERT_EQ(".com", reverseWords<std::string>("com.", "."));
    ASSERT_EQ("com.", reverseWords<std::string>(".com", "."));
}

TEST(Algorithm_reverseWords, empty_string)
{
    ASSERT_EQ("", reverseWords<std::string>("", "."));
}

TEST(Algorithm_reverseWords, no_separators)
{
    ASSERT_EQ("com", reverseWords<std::string>("com", "."));
}

TEST(Algorithm_reverseWords, only_separators)
{
    ASSERT_EQ("...", reverseWords<std::string>("...", "."));
    ASSERT_EQ(".", reverseWords<std::string>(".", "."));
}

//-------------------------------------------------------------------------------------------------
// countElementsWithPrefix

TEST(Algorithm_countElementsWithPrefix, all)
{
    std::map<std::string, int> m;
    m.emplace("a", 0);
    m.emplace("aaabbbccc", 0);
    m.emplace("aaabbbddd", 0);
    m.emplace("aaabbbeee", 0);
    m.emplace("aaafff", 0);

    ASSERT_EQ(4U, countElementsWithPrefix(m, "aaa"));
    ASSERT_EQ(5U, countElementsWithPrefix(m, "a"));
    ASSERT_EQ(3U, countElementsWithPrefix(m, "aaab"));
    ASSERT_EQ(1U, countElementsWithPrefix(m, "aaaf"));
    ASSERT_EQ(1U, countElementsWithPrefix(m, "aaabbbddd"));
    ASSERT_EQ(1U, countElementsWithPrefix(m, "aaabbbdd"));
    ASSERT_EQ(0U, countElementsWithPrefix(m, "aaabbbddd111"));
    ASSERT_EQ(1U, countElementsWithPrefix(m, "aaabbbe"));
    ASSERT_EQ(1U, countElementsWithPrefix(m, "aaabbbc"));
}

//-------------------------------------------------------------------------------------------------
// findFirstElementWithPrefix

TEST(Algorithm_findFirstElementWithPrefix, all)
{
    std::map<std::string, int> m;
    m.emplace("a", 1);
    m.emplace("aaabbbccc", 2);
    m.emplace("aaabbbddd", 3);
    m.emplace("aaabbbeee", 4);
    m.emplace("aaafff", 5);

    ASSERT_EQ(m.end(), findFirstElementWithPrefix(m, "c"));
    ASSERT_EQ(m.end(), findFirstElementWithPrefix(m, "aaafff1"));
    ASSERT_EQ(1, findFirstElementWithPrefix(m, "a")->second);
    ASSERT_EQ(2, findFirstElementWithPrefix(m, "aa")->second);
    ASSERT_EQ(4, findFirstElementWithPrefix(m, "aaabbbe")->second);
    ASSERT_EQ(5, findFirstElementWithPrefix(m, "aaaff")->second);
}

//-------------------------------------------------------------------------------------------------
// equalRangeByPrefix

TEST(Algorithm_equalRangeByPrefix, all)
{
    std::map<std::string, int> m;
    m.emplace("a", 1);
    m.emplace("aaabbbccc", 2);
    m.emplace("aaabbbddd", 3);
    m.emplace("aaabbbeee", 4);
    m.emplace("aaafff", 5);

    auto range = equalRangeByPrefix(m, "a");
    ASSERT_EQ(m.begin(), range.first);
    ASSERT_EQ(m.end(), range.second);

    range = equalRangeByPrefix(m, "aaab");
    ASSERT_EQ(2, range.first->second);
    ASSERT_EQ(5, range.second->second);

    range = equalRangeByPrefix(m, "aaaf");
    ASSERT_EQ(5, range.first->second);
    ASSERT_EQ(m.end(), range.second);

    range = equalRangeByPrefix(m, "c");
    ASSERT_EQ(m.end(), range.first);
    ASSERT_EQ(m.end(), range.second);
}

} // namespace test
} // namespace utils
} // namespace nx
