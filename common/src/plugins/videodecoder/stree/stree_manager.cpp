/**********************************************************
* May 17, 2016
* a.kolesnikov
***********************************************************/

#include "stree_manager.h"

#include <QtCore/QFile>
#include <QtCore/QVariant>
#include <QtXml/QXmlInputSource>
#include <QtXml/QXmlSimpleReader>

#include <nx/utils/log/log.h>

#include "streesaxhandler.h"


namespace stree {

StreeManager::StreeManager(
    const stree::ResourceNameSet& resourceNameSet,
    const QString& xmlFilePath) throw(std::runtime_error)
:
    m_attrNameSet(resourceNameSet),
    m_xmlFilePath(xmlFilePath)
{
    loadStree();
}

void StreeManager::search(
    const stree::AbstractResourceReader& input,
    stree::AbstractResourceWriter* const output) const
{
    m_stree->get(input, output);
}

const stree::ResourceNameSet& StreeManager::resourceNameSet() const
{
    return m_attrNameSet;
}

void StreeManager::loadStree() throw(std::runtime_error)
{
    stree::SaxHandler xmlHandler(m_attrNameSet);

    QXmlSimpleReader reader;
    reader.setContentHandler(&xmlHandler);
    reader.setErrorHandler(&xmlHandler);

    QFile xmlFile(m_xmlFilePath);
    if (!xmlFile.open(QIODevice::ReadOnly))
        throw std::runtime_error("Failed to open stree xml file " +
            m_xmlFilePath.toStdString() + ": " + xmlFile.errorString().toStdString());

    QXmlInputSource input(&xmlFile);
    NX_LOG(lit("Parsing stree xml file (%1)").arg(m_xmlFilePath), cl_logDEBUG1);
    if (!reader.parse(&input))
        throw std::runtime_error("Failed to parse stree xml file " +
            m_xmlFilePath.toStdString() + ": " + xmlHandler.errorString().toStdString());

    m_stree = xmlHandler.releaseTree();
}

}   //namespace stree
