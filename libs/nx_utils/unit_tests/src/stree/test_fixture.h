// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/utils/stree/stree_manager.h>

namespace nx::utils::stree::test {

class StreeFixture:
    public ::testing::Test
{
public:
    enum Attributes
    {
        intAttr = 1,
        strAttr,
        outAttr,
    };

    StreeFixture();

protected:
    bool prepareTree(const char* xmlDataStr, int parseFlags);
    const std::unique_ptr<AbstractNode>& streeRoot() const;

    /**
     * @return Attributes::outAttr value.
     */
    std::string search(const ResourceContainer& inputData);

private:
    ResourceNameSet m_nameSet;
    std::unique_ptr<AbstractNode> m_streeRoot;
};

} // namespace nx::utils::stree::test
