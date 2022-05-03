// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/data_structures/partitioned_concurrent_hash_map.h>

namespace nx::utils::test {

TEST(PartitionedConcurrentHashMap, main)
{
    std::vector<std::pair<int, std::string>> vals{
        {1, {"1"}}, {2, {"2"}}, {3, {"3"}}};

    PartitionedConcurrentHashMap<int, std::string> dict;
    ASSERT_TRUE(dict.empty());

    for (const auto& [key, val]: vals)
        ASSERT_TRUE(dict.emplace(key, val));
    ASSERT_EQ(vals.size(), dict.size());
    ASSERT_FALSE(dict.empty());

    for (const auto& [key, val]: vals)
        ASSERT_TRUE(dict.contains(key));

    for (const auto& [key, val]: vals)
    {
        auto foundVal = dict.find(key);
        ASSERT_TRUE(foundVal);
        ASSERT_EQ(val, *foundVal);
    }

    for (const auto& [key, val]: vals)
    {
        auto takenVal = dict.take(key);
        ASSERT_TRUE(takenVal);
        ASSERT_EQ(val, *takenVal);
        ASSERT_FALSE(dict.contains(key));
    }

    ASSERT_TRUE(dict.empty());
}

TEST(PartitionedConcurrentHashMap, modify)
{
    PartitionedConcurrentHashMap<int, std::string> dict;
    dict.emplace(1, "1");

    dict.modify(1, [](auto& val) { val = "11"; });
    ASSERT_EQ("11", *dict.find(1));

    dict.modify(2, [](auto& val) { val = "22"; });
    ASSERT_EQ("22", *dict.find(2));
}

} // namespace nx::utils::test
