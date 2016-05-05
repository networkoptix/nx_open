/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#include "stree_manager.h"

#include <QtCore/QFile>
#include <QtCore/QVariant>
#include <QtXml/QXmlInputSource>
#include <QtXml/QXmlSimpleReader>

#include <plugins/videodecoder/stree/streesaxhandler.h>
#include <nx/utils/log/log.h>
#include <utils/common/model_functions.h>

#include "cdb_ns.h"


namespace nx {
namespace cdb {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(StreeOperation,
    (StreeOperation::authentication, "authentication")
    (StreeOperation::authorization, "authorization")
)


StreeManager::StreeManager(const conf::Auth& authSettings) throw(std::runtime_error)
:
    m_authSettings(authSettings)
{
    loadStree();
}

void StreeManager::search(
    StreeOperation operation,
    const stree::AbstractResourceReader& input,
    stree::AbstractResourceWriter* const output) const
{
    stree::SingleResourceContainer operationRes(
        attr::operation, QnLexical::serialized(operation));
    stree::MultiSourceResourceReader realInput(operationRes, input);

    m_stree->get(realInput, output);
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

    QFile xmlFile(m_authSettings.rulesXmlPath);
    if (!xmlFile.open(QIODevice::ReadOnly))
        throw std::runtime_error("Failed to open stree xml file " +
            m_authSettings.rulesXmlPath.toStdString() + ": " + xmlFile.errorString().toStdString());

    QXmlInputSource input(&xmlFile);
    NX_LOG(lit("Parsing stree xml file (%1)").arg(m_authSettings.rulesXmlPath), cl_logDEBUG1);
    if (!reader.parse(&input))
        throw std::runtime_error("Failed to parse stree xml file " +
            m_authSettings.rulesXmlPath.toStdString() + ": " + xmlHandler.errorString().toStdString());

    m_stree = xmlHandler.releaseTree();
}

}   //cdb
}   //nx
