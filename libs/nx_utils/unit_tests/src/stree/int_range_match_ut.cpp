// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <cstring>
#include <string>

#include "test_fixture.h"

namespace nx::utils::stree::test {

using ::nx::utils::stree::AbstractNode;
using ::nx::utils::stree::AttributeDictionary;
using ::nx::utils::stree::StreeManager;

class StreeRangeMatch:
    public StreeFixture
{
protected:
    std::string getOutAttr(std::string strAttrVal)
    {
        AttributeDictionary inputData;
        inputData.put(Attributes::strAttr, strAttrVal);
        return search(inputData);
    }

    std::string getOutAttr(int intAttrVal)
    {
        AttributeDictionary inputData;
        inputData.put(Attributes::intAttr, std::to_string(intAttrVal));
        return search(inputData);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(StreeRangeMatch, intValue)
{
    const char* testXmlData =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<sequence>\n"
        "    <set resName=\"outAttr\" resValue=\"default\"/>\n"
        "    <condition resName=\"intAttr\" matchType=\"intRange\">\n"
        "        <set value=\"0-9\" resName=\"outAttr\" resValue=\"first\"/>\n"
        "        <set value=\"0-9\" resName=\"outAttr\" resValue=\"xxx\"/>\n"
        "        <set value=\"20-30\" resName=\"outAttr\" resValue=\"second\"/>\n"
        "    </condition>\n"
        "</sequence>\n";

    ASSERT_TRUE(prepareTree(testXmlData));

    ASSERT_EQ("first", getOutAttr(0));
    ASSERT_EQ("first", getOutAttr(9));
    ASSERT_EQ("first", getOutAttr(5));

    ASSERT_EQ("default", getOutAttr(-1));
    ASSERT_EQ("default", getOutAttr(10));
    ASSERT_EQ("default", getOutAttr(19));

    ASSERT_EQ("second", getOutAttr(20));
    ASSERT_EQ("second", getOutAttr(30));
    ASSERT_EQ("second", getOutAttr(25));

    ASSERT_EQ("default", getOutAttr(31));
    ASSERT_EQ("default", getOutAttr(100));
}

TEST_F(StreeRangeMatch, strValue)
{
    const char* testXmlData =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<sequence>\n"
        "    <set resName=\"outAttr\" resValue=\"default\"/>\n"
        "    <condition resName=\"strAttr\" matchType=\"range\">\n"
        "        <set value=\"aa-ad\" resName=\"outAttr\" resValue=\"first\"/>\n"
        "        <set value=\"bb-cc\" resName=\"outAttr\" resValue=\"second\"/>\n"
        "    </condition>\n"
        "</sequence>\n";

    ASSERT_TRUE(prepareTree(testXmlData));

    ASSERT_EQ("default", getOutAttr("a0"));

    ASSERT_EQ("first", getOutAttr("aa"));
    ASSERT_EQ("first", getOutAttr("ab"));
    ASSERT_EQ("first", getOutAttr("ad"));

    ASSERT_EQ("default", getOutAttr("ae"));
    ASSERT_EQ("default", getOutAttr("ba"));

    ASSERT_EQ("second", getOutAttr("bb"));
    ASSERT_EQ("second", getOutAttr("cb"));
    ASSERT_EQ("second", getOutAttr("cc"));

    ASSERT_EQ("default", getOutAttr("cd"));
    ASSERT_EQ("default", getOutAttr("d"));
    ASSERT_EQ("default", getOutAttr("ff"));
}

//-------------------------------------------------------------------------------------------------

class StreeGreaterMatch:
    public StreeFixture
{
protected:
    virtual void SetUp() override
    {
        const char* testXmlData =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<sequence>\n"
            "    <set resName=\"outAttr\" resValue=\"default\"/>\n"
            "    <condition resName=\"intAttr\" matchType=\"intGreater\">\n"
            "        <set value=\"10\" resName=\"outAttr\" resValue=\">10\"/>\n"
            "        <set value=\"25\" resName=\"outAttr\" resValue=\">25\"/>\n"
            "        <set value=\"100\" resName=\"outAttr\" resValue=\">100\"/>\n"
            "    </condition>\n"
            "</sequence>\n";

        ASSERT_TRUE(prepareTree(testXmlData));
    }
};

TEST_F(StreeGreaterMatch, works)
{
    ASSERT_EQ("default", search({{"intAttr", "-12"}}));
    ASSERT_EQ("default", search({{"intAttr", "9"}}));
    ASSERT_EQ("default", search({{"intAttr", "10"}}));
    ASSERT_EQ(">10", search({{"intAttr", "15"}}));
    ASSERT_EQ(">25", search({{"intAttr", "26"}}));
    ASSERT_EQ(">25", search({{"intAttr", "100"}}));
    ASSERT_EQ(">100", search({{"intAttr", "120"}}));
}

TEST_F(StreeGreaterMatch, example_from_doc)
{
    static constexpr char kTreeDoc[] =
R"xml(<?xml version="1.0" encoding="utf-8"?>
<condition resName="colorName">
    <sequence value="black">
        <set resName="r" resValue="0"/>
        <set resName="g" resValue="0"/>
        <set resName="b" resValue="0"/>
    </sequence>
    <sequence value="white">
        <set resName="r" resValue="255"/>
        <set resName="g" resValue="255"/>
        <set resName="b" resValue="255"/>
    </sequence>
</condition>
)xml";

    // Loading tree into memory.
    auto treeRoot = nx::utils::stree::StreeManager::loadStree(kTreeDoc);

    // Querying the tree.
    nx::utils::stree::AttributeDictionary inAttrs{{"colorName", "black"}};
    nx::utils::stree::AttributeDictionary outAttrs;
    treeRoot->get(inAttrs, &outAttrs);

    // Testing the result.
    assert(outAttrs.get<int>("r") == 0);
    assert(outAttrs.get<int>("g") == 0);
    assert(outAttrs.get<int>("b") == 0);
}

} // namespace nx::utils::stree::test
