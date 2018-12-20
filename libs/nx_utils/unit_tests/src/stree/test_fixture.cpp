#include "test_fixture.h"

#include <cstring>

#include <QtCore/QBuffer>

namespace nx::utils::stree::test {

StreeFixture::StreeFixture()
{
    m_nameSet.registerResource(intAttr, "intAttr", QVariant::Int);
    m_nameSet.registerResource(strAttr, "strAttr", QVariant::String);
    m_nameSet.registerResource(outAttr, "outAttr", QVariant::String);
}

bool StreeFixture::prepareTree(
    const char* xmlDataStr,
    int parseFlags)
{
    auto xmlData = QByteArray::fromRawData(xmlDataStr, std::strlen(xmlDataStr));
    QBuffer xmlDataSource(&xmlData);
    m_streeRoot = StreeManager::loadStree(
        &xmlDataSource,
        m_nameSet,
        parseFlags);
    return m_streeRoot != nullptr;
}

const std::unique_ptr<AbstractNode>& StreeFixture::streeRoot() const
{
    return m_streeRoot;
}

} // namespace nx::utils::stree::test
