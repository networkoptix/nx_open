// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/reflect/instrument.h>

#include "types.h"

namespace nx::reflect::test {

template<typename FormatTypeSet>
class FormatAcceptance:
    public ::testing::Test
{
protected:
    A prepareData()
    {
        A a;
        a.num = 47;
        a.str = "Hello";
        a.b.num = 12;
        a.b.str = "BBB";
        a.b.text = "Hello";
        a.b.numbers.push_back(1);
        a.b.numbers.push_back(2);
        a.b.numbers.push_back(3);
        a.b.strings.push_back("a");
        a.b.strings.push_back("bb");
        a.b.strings.push_back("ccc");
        a.bs.push_back(a.b);
        a.bs.push_back(a.b);
        a.c.str = "Hello";
        a.keyToValue.emplace("key1", "value1");
        a.keyToValue.emplace("key2", "value2");
        return a;
    }

    template<typename Type>
    void assertSerializeAndDeserializeAreSymmetric(const Type& value)
    {
        const auto serializedData = FormatTypeSet::serialize(value);
        //std::cout << serializeData << std::endl;

        Type parsed;
        ASSERT_TRUE(FormatTypeSet::deserialize(serializedData, &parsed));
        ASSERT_EQ(value, parsed);

        const auto serializedData2 = FormatTypeSet::serialize(parsed);
        ASSERT_EQ(serializedData, serializedData2);
    }
};

TYPED_TEST_SUITE_P(FormatAcceptance);

TYPED_TEST_P(FormatAcceptance, serialize_and_deserialize_are_symmetric)
{
    this->assertSerializeAndDeserializeAreSymmetric(this->prepareData());
}

TYPED_TEST_P(FormatAcceptance, type_with_base_without_base)
{
    D d;
    (A&)d = this->prepareData();
    d.ddd = rand();

    this->assertSerializeAndDeserializeAreSymmetric(d);
}

TYPED_TEST_P(FormatAcceptance, type_with_base_with_base)
{
    E e;
    (A&)e = this->prepareData();
    e.ddd = rand();
    e.e = rand();

    this->assertSerializeAndDeserializeAreSymmetric(e);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(FormatAcceptance);
REGISTER_TYPED_TEST_SUITE_P(FormatAcceptance,
    serialize_and_deserialize_are_symmetric,
    type_with_base_without_base,
    type_with_base_with_base);

} // namespace nx::reflect::test
