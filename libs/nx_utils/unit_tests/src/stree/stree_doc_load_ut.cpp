// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_fixture.h"

namespace nx::utils::stree::test {

class StreeDocLoad:
    public StreeFixture
{
protected:
    void loadDocumentWithUnknownResources()
    {
        const char document[] =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<sequence>\n"
                "<set resName=\"outAttr\" resValue=\"default\"/>\n"
                "<set resName=\"unknown1\" resValue=\"AAA\"/>\n"
                "<condition resName=\"unknown2\">\n"
                    "<set value=\"unreachable\" resName=\"outAttr\" resValue=\"invalid\"/>\n"
                "</condition>\n"
                "<condition resName=\"intAttr\" matchType=\"equal\">\n"
                    "<set value=\"1\" resName=\"outAttr\" resValue=\"one\"/>\n"
                    "<set value=\"11\" resName=\"outAttr\" resValue=\"eleven\"/>\n"
                    "<set value=\"22\" resName=\"outAttr\" resValue=\"twenty two\"/>\n"
                "</condition>\n"
            "</sequence>\n";

        ASSERT_TRUE(prepareTree(document));
    }

    void assertDocumentDataIsAvailable()
    {
        AttributeDictionary data;
        data.put(Attributes::intAttr, "11");
        streeRoot()->get(data, &data);

        const auto foundValue = data.get<std::string>(Attributes::outAttr);
        ASSERT_TRUE(static_cast<bool>(foundValue));
        ASSERT_EQ("eleven", *foundValue);
    }
};

TEST_F(StreeDocLoad, nodes_with_unknown_resource_id_are_ignored)
{
    loadDocumentWithUnknownResources();
    assertDocumentDataIsAvailable();
}

} // namespace nx::utils::stree::test
