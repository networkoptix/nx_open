// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/stun/message.h>

namespace nx::network::stun::test {

TEST(StunMessage, attribute_is_replaced)
{
    Message msg;

    msg.newAttribute<attrs::UserName>("abc");
    ASSERT_EQ("abc", msg.getAttribute<attrs::UserName>()->getString());

    msg.newAttribute<attrs::UserName>("def");
    ASSERT_EQ(1U, msg.attributeCount());
    ASSERT_EQ("def", msg.getAttribute<attrs::UserName>()->getString());

    msg.eraseAttribute(attrs::userName);
    ASSERT_EQ(0U, msg.attributeCount());
    ASSERT_EQ(nullptr, msg.getAttribute<attrs::UserName>());
}

TEST(StunMessage, attribute_order_is_preserved)
{
    Message msg;

    msg.addAttribute(std::make_unique<attrs::Unknown>(123, "123"));
    msg.addAttribute(std::make_unique<attrs::Unknown>(11, "11"));
    msg.addAttribute(std::make_unique<attrs::Unknown>(523, "523"));

    std::vector<int> order;
    msg.forEachAttribute([&order](const attrs::Attribute* attr) { order.push_back(attr->getType()); });

    std::vector<int> expectedOrder{123, 11, 523};
    ASSERT_EQ(expectedOrder, order);
}

} // namespace nx::network::stun::test
