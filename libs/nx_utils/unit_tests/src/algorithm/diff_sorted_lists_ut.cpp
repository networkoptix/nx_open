// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <set>

#include <gtest/gtest.h>

#include <nx/utils/algorithm/diff_sorted_lists.h>

namespace nx::utils::algorithm::test {

using namespace std;

class full_difference:
    public ::testing::Test
{
protected:
    template<typename Left, typename Right>
    void calc(const Left& left, const Right& right)
    {
        full_difference_2(
            left.begin(), left.end(), right.begin(), right.end(),
            std::back_inserter(m_leftOnly), std::back_inserter(m_rightOnly),
            std::back_inserter(m_eq));
    }

    const vector<int>& leftOnly() const { return m_leftOnly; }
    const vector<int>& rightOnly() const { return m_rightOnly; }
    const vector<int>& eq() const { return m_eq; }

private:
    vector<int> m_leftOnly;
    vector<int> m_rightOnly;
    vector<int> m_eq;
};

TEST_F(full_difference, intersecting_lists)
{
    calc(
        set<int>{1, 5, 7, 10, 12},
        set<int>{3, 4, 5, 6, 10, 11, 14});

    ASSERT_EQ(vector<int>({1, 7, 12}), leftOnly());
    ASSERT_EQ(vector<int>({3, 4, 6, 11, 14}), rightOnly());
    ASSERT_EQ(vector<int>({5, 10}), eq());
}

TEST_F(full_difference, trailing_left)
{
    calc(
        set<int>{1, 2, 3},
        set<int>{11, 12, 13});

    ASSERT_EQ(vector<int>({ 1, 2, 3 }), leftOnly());
    ASSERT_EQ(vector<int>({ 11, 12, 13 }), rightOnly());
    ASSERT_EQ(vector<int>({}), eq());
}

TEST_F(full_difference, trailing_right)
{
    calc(
        set<int>{11, 12, 13},
        set<int>{1, 2, 3});

    ASSERT_EQ(vector<int>({ 11, 12, 13 }), leftOnly());
    ASSERT_EQ(vector<int>({ 1, 2, 3 }), rightOnly());
    ASSERT_EQ(vector<int>({}), eq());
}

TEST_F(full_difference, left_empty)
{
    calc(
        set<int>{},
        set<int>{3, 4, 5});

    ASSERT_EQ(vector<int>(), leftOnly());
    ASSERT_EQ(vector<int>({3, 4, 5}), rightOnly());
    ASSERT_EQ(vector<int>(), eq());
}

TEST_F(full_difference, right_empty)
{
    calc(
        set<int>{3, 4, 5},
        set<int>{});

    ASSERT_EQ(vector<int>({ 3, 4, 5 }), leftOnly());
    ASSERT_EQ(vector<int>(), rightOnly());
    ASSERT_EQ(vector<int>(), eq());
}

TEST_F(full_difference, both_empty)
{
    calc(
        set<int>{},
        set<int>{3, 4, 5});

    ASSERT_EQ(vector<int>(), leftOnly());
    ASSERT_EQ(vector<int>({ 3, 4, 5 }), rightOnly());
    ASSERT_EQ(vector<int>(), eq());
}

TEST_F(full_difference, equal)
{
    calc(
        set<int>{3, 4, 5},
        set<int>{3, 4, 5});

    ASSERT_EQ(vector<int>(), leftOnly());
    ASSERT_EQ(vector<int>(), rightOnly());
    ASSERT_EQ(vector<int>({ 3, 4, 5 }), eq());
}

} // namespace nx::utils::algorithm::test
