/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include "cloud_modules_xml_sax_handler.h"

#include <common/common_globals.h>


namespace nx {
namespace cdb {
namespace cl {

CloudModulesXmlHandler::CloudModulesXmlHandler()
:
    m_state(init)
{
}

CloudModulesXmlHandler::~CloudModulesXmlHandler()
{
}

bool CloudModulesXmlHandler::startDocument()
{
    return true;
}

bool CloudModulesXmlHandler::endDocument()
{
    return true;
}

bool CloudModulesXmlHandler::startElement(
    const QString& /*namespaceURI*/,
    const QString& /*localName*/,
    const QString& qName,
    const QXmlAttributes& atts)
{
    switch (m_state)
    {
        case init:
            if (qName == "modules")
                m_state = readingModules;
            else
                return false;
            return true;

        case readingModules:
            m_state = readingModuleEndpoints;
            m_currentModule = qName;
            return true;

        case readingModuleEndpoints:
            if (qName != "endpoint")
                return false;
            m_state = readingModuleEndpoint;
            return true;
    }

    return false;
}

bool CloudModulesXmlHandler::characters(const QString& ch)
{
    if (m_state == readingModuleEndpoint)
        m_endpoints[m_currentModule].push_back(ch);
    return true;
}

bool CloudModulesXmlHandler::endElement(const QString& namespaceURI, const QString& localName, const QString& qName)
{
    switch (m_state)
    {
        case readingModuleEndpoint:
            m_state = readingModuleEndpoints;
            return true;

        case readingModuleEndpoints:
            m_state = readingModules;
            return true;

        case readingModules:
            m_state = init;
            return true;

        case init:
            return true;
    }

    return false;
}

QString CloudModulesXmlHandler::errorString() const
{
    return m_errorDescription;
}

bool CloudModulesXmlHandler::error(const QXmlParseException& exception)
{
    m_errorDescription = lit("Parse error. line %1, col %2, parser message: %3").
        arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

bool CloudModulesXmlHandler::fatalError(const QXmlParseException& exception)
{
    m_errorDescription = lit("Parse error. line %1, col %2, parser message: %3").
        arg(exception.lineNumber()).arg(exception.columnNumber()).arg(exception.message());
    return false;
}

std::list<SocketAddress> CloudModulesXmlHandler::moduleUrls(QString moduleName) const
{
    auto it = m_endpoints.find(moduleName);
    return it == m_endpoints.end()
        ? std::list<SocketAddress>()
        : it->second;
}

}   //cl
}   //cdb
}   //nx
