#include "stree_manager.h"

#include <QtCore/QFile>
#include <QtCore/QVariant>
#include <QtXml/QXmlInputSource>
#include <QtXml/QXmlSimpleReader>

#include <nx/utils/log/log.h>

#include "streesaxhandler.h"

namespace nx {
namespace utils {
namespace stree {

StreeManager::StreeManager(
    const nx::utils::stree::ResourceNameSet& resourceNameSet,
    const QString& xmlFilePath) noexcept(false)
:
    m_attrNameSet(resourceNameSet),
    m_xmlFilePath(xmlFilePath)
{
    loadStree();
}

void StreeManager::search(
    const nx::utils::stree::AbstractResourceReader& input,
    nx::utils::stree::AbstractResourceWriter* const output) const
{
    m_stree->get(input, output);
}

const nx::utils::stree::ResourceNameSet& StreeManager::resourceNameSet() const
{
    return m_attrNameSet;
}

std::unique_ptr<nx::utils::stree::AbstractNode> StreeManager::loadStree(
    QIODevice* const dataSource,
    const nx::utils::stree::ResourceNameSet& resourceNameSet)
{
    nx::utils::stree::SaxHandler xmlHandler(resourceNameSet);

    QXmlSimpleReader reader;
    reader.setContentHandler(&xmlHandler);
    reader.setErrorHandler(&xmlHandler);

    QXmlInputSource input(dataSource);
    if (!reader.parse(&input))
    {
        NX_LOG(lit("Failed to parse stree xml: %1").arg(xmlHandler.errorString()), cl_logWARNING);
        return nullptr;
    }

    return xmlHandler.releaseTree();
}

void StreeManager::loadStree() noexcept(false)
{
    QFile xmlFile(m_xmlFilePath);
    if (!xmlFile.open(QIODevice::ReadOnly))
        throw std::runtime_error("Failed to open stree xml file " +
            m_xmlFilePath.toStdString() + ": " + xmlFile.errorString().toStdString());
    NX_LOG(lit("Parsing stree xml file (%1)").arg(m_xmlFilePath), cl_logDEBUG1);
    m_stree = loadStree(&xmlFile, m_attrNameSet);
    if (!m_stree)
        throw std::runtime_error("Failed to parse stree xml file " + m_xmlFilePath.toStdString());
}

} // namespace stree
} // namespace utils
} // namespace nx
