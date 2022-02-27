// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/utils/resource_params_data.h>

namespace nx::vms::utils::test {

TEST(ResourceParamsData, chooseFirstFromEqual)
{
    const std::vector<ResourceParamsData> datas = {
        {"1", R"json({"version": "1", "data": []})json"},
        {"2", R"json({"version": "1", "data": []})json"},
        {"3", R"json({"version": "1", "data": []})json"},
    };
    ASSERT_EQ("1", ResourceParamsData::getWithGreaterVersion(datas).location);
}

TEST(ResourceParamsData, chooseFirstFromReverse)
{
    const std::vector<ResourceParamsData> datas = {
        {"1", R"json({"version": "3", "data": []})json"},
        {"2", R"json({"version": "2", "data": []})json"},
        {"3", R"json({"version": "1", "data": []})json"},
    };
    ASSERT_EQ("1", ResourceParamsData::getWithGreaterVersion(datas).location);
}

TEST(ResourceParamsData, chooseLastFromDirect)
{
    const std::vector<ResourceParamsData> datas = {
        {"1", R"json({"version": "1", "data": []})json"},
        {"2", R"json({"version": "2", "data": []})json"},
        {"3", R"json({"version": "3", "data": []})json"},
    };
    ASSERT_EQ("3", ResourceParamsData::getWithGreaterVersion(datas).location);
}

TEST(ResourceParamsData, chooseMiddleFromReverse)
{
    const std::vector<ResourceParamsData> datas = {
        {"1", R"json({"version": "1", "data": []})json"},
        {"2", R"json({"version": "3", "data": []})json"},
        {"3", R"json({"version": "2", "data": []})json"},
    };
    ASSERT_EQ("2", ResourceParamsData::getWithGreaterVersion(datas).location);
}

TEST(ResourceParamsData, chooseMiddleFromDirect)
{
    const std::vector<ResourceParamsData> datas = {
        {"1", R"json({"version": "2", "data": []})json"},
        {"2", R"json({"version": "3", "data": []})json"},
        {"3", R"json({"version": "1", "data": []})json"},
    };
    ASSERT_EQ("2", ResourceParamsData::getWithGreaterVersion(datas).location);
}

} // namespace nx::vms::utils::test
