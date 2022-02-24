// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <map>
#include <string>

#include <nx/preprocessor/count.h>
#include <nx/preprocessor/for_each.h>

namespace nx::preprocessor::test {

TEST(Preprocessor, Count)
{
    ASSERT_EQ(NX_PP_ARGS_COUNT(a), 1);
    ASSERT_EQ(NX_PP_ARGS_COUNT(a, b), 2);
}

TEST(Preprocessor, ForEachSimple)
{
    #define FOR_EACH_TEST(_, x) + x * x

    const int result = 0 NX_PP_FOR_EACH(FOR_EACH_TEST, _, 1, 2, 3, 4);
    ASSERT_EQ(result, 1 + 2 * 2 + 3 * 3 + 4 * 4);

    #undef FOR_EACH_TEST
}

TEST(Preprocessor, ForEachWithData)
{
    #define FOR_EACH_TEST(a, x) a += x * x;

    int result = 0;
    NX_PP_FOR_EACH(FOR_EACH_TEST, result, 1, 2, 3, 4)
    ASSERT_EQ(result, 1 + 2 * 2 + 3 * 3 + 4 * 4);

    #undef FOR_EACH_TEST
}

TEST(Preprocessor, ForEachComplexArguments)
{
    #define FOR_EACH_TEST(_, x) {NX_PP_EXPAND x},

    const std::map<std::string, int> testMap{
        NX_PP_FOR_EACH(FOR_EACH_TEST, _, ("a", 1), ("b", 2), ("c", 3))
    };

    const std::map<std::string, int> controlMap{{"a", 1}, {"b", 2}, {"c", 3}};

    ASSERT_EQ(testMap, controlMap);

    #undef FOR_EACH_TEST
}

} // namespace nx::preprocessor::test
