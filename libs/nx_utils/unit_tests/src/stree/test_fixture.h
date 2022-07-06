// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/utils/stree/stree_manager.h>

namespace nx::utils::stree::test {

namespace Attributes {

static constexpr char intAttr[] = "intAttr";
static constexpr char strAttr[] = "strAttr";
static constexpr char outAttr[] = "outAttr";

} // namespace Attributes

class StreeFixture:
    public ::testing::Test
{
protected:
    bool prepareTree(const char* xmlDataStr);
    const std::unique_ptr<AbstractNode>& streeRoot() const;

    /**
     * @return Attributes::outAttr value.
     */
    std::string search(const AttributeDictionary& inputData);

private:
    std::unique_ptr<AbstractNode> m_streeRoot;
};

} // namespace nx::utils::stree::test
