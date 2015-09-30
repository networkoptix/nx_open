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
#include <utils/common/log.h>
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
    prepareNamespace();
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

void StreeManager::prepareNamespace()
{
    m_rns.registerResource(attr::operation, "operation", QVariant::String);
    m_rns.registerResource(attr::accountID, "accountID", QVariant::String);
    m_rns.registerResource(attr::systemID, "systemID", QVariant::String);
    m_rns.registerResource(attr::authenticated, "authenticated", QVariant::Bool);
    m_rns.registerResource(attr::authorized, "authorized", QVariant::Bool);
    m_rns.registerResource(attr::ha1, "ha1", QVariant::String);
    m_rns.registerResource(attr::userName, "user.name", QVariant::String);
    m_rns.registerResource(attr::userPassword, "user.password", QVariant::String);
    m_rns.registerResource(attr::authAccountRightsOnSystem, "authAccountRightsOnSystem", QVariant::String);
    m_rns.registerResource(attr::accountStatus, "account.status", QVariant::Int);
    m_rns.registerResource(attr::entity, "entity", QVariant::String);
    m_rns.registerResource(attr::action, "action", QVariant::String);
    m_rns.registerResource(attr::secureSource, "secureSource", QVariant::Bool);

    m_rns.registerResource(attr::socketIntfIP, "socket.intf.ip", QVariant::String);
    m_rns.registerResource(attr::socketRemoteIP, "socket.remoteIP", QVariant::String);

    m_rns.registerResource(attr::requestPath, "request.path", QVariant::String);
}

void StreeManager::loadStree() throw(std::runtime_error)
{
    stree::SaxHandler xmlHandler(m_rns);

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
            m_authSettings.rulesXmlPath.toStdString() + ": " + xmlFile.errorString().toStdString());

    m_stree = xmlHandler.releaseTree();
}


}   //cdb
}   //nx
