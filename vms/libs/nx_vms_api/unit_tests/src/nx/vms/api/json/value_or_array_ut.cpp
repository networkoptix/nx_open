// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api::json::test {

class ValueOrArrayTest: public ::testing::Test
{
public:
    template<class T>
    void checkEqual(const ValueOrArray<T>& voa, std::string_view str)
    {
        EXPECT_EQ(QJson::serialized(voa), str);

        ValueOrArray<T> temp;
        EXPECT_TRUE(QJson::deserialize(str, &temp));
        EXPECT_EQ(temp, voa);
    }
};

TEST_F(ValueOrArrayTest, value)
{
    checkEqual(ValueOrArray<int>{1}, "1");
    checkEqual(ValueOrArray<QString>{"foo"}, "\"foo\"");
}

TEST_F(ValueOrArrayTest, array)
{
    checkEqual(ValueOrArray<int>{1, 2, 3}, "[1,2,3]");
    checkEqual(ValueOrArray<QString>{"foo", "bar"}, "[\"foo\",\"bar\"]");
}

} // namespace nx::vms::api::json::test
