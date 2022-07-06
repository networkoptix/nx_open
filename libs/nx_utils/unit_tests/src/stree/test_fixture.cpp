// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_fixture.h"

#include <cstring>

namespace nx::utils::stree::test {

bool StreeFixture::prepareTree(const char* xmlDataStr)
{
    auto xmlData = QByteArray::fromRawData(xmlDataStr, (int) std::strlen(xmlDataStr));
    m_streeRoot = StreeManager::loadStree(xmlData);
    return m_streeRoot != nullptr;
}

const std::unique_ptr<AbstractNode>& StreeFixture::streeRoot() const
{
    return m_streeRoot;
}

std::string StreeFixture::search(const AttributeDictionary& inputData)
{
    AttributeDictionary outputData;
    streeRoot()->get(makeMultiReader(inputData, outputData), &outputData);
    if (auto outVal = outputData.get<std::string>(Attributes::outAttr))
        return *outVal;
    return std::string();
}

} // namespace nx::utils::stree::test
