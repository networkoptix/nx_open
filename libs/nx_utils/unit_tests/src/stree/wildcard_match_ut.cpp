// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include "test_fixture.h"

namespace nx::utils::stree::test {

static constexpr char kTestXmlData[] = R"xml(<?xml version="1.0" encoding="utf-8"?>
<sequence>
    <set resName="outAttr" resValue="default"/>
    <condition resName="strAttr" matchType="wildcard">
        <set value="/foo/rename" resName="outAttr" resValue="rename"/>
        <set value="/foo/razraz" resName="outAttr" resValue="razraz"/>
        <set value="/foo/?*" resName="outAttr" resValue="other"/>
    </condition>
</sequence>
)xml";

class StreeWildcardMatch:
    public StreeFixture
{
    using base_type = StreeFixture;

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(prepareTree(kTestXmlData));
    }

    std::string search(const std::string& strAttrVal)
    {
        AttributeDictionary inputData;
        inputData.put(Attributes::strAttr, strAttrVal);
        return base_type::search(inputData);
    }
};

TEST_F(StreeWildcardMatch, longest_mask_is_chosen_first)
{
    ASSERT_EQ("rename", search("/foo/rename"));
    ASSERT_EQ("razraz", search("/foo/razraz"));
    ASSERT_EQ("other", search("/foo/unknown"));
}

} // namespace nx::utils::stree::test
