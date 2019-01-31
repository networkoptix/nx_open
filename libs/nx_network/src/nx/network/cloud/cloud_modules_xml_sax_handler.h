#pragma once

#include <list>
#include <map>

#include <QtXml/QXmlDefaultHandler>

#include <nx/network/socket_common.h>

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API CloudModulesXmlHandler:
    public QXmlDefaultHandler
{
public:
    CloudModulesXmlHandler();
    virtual ~CloudModulesXmlHandler() = default;

    virtual bool startDocument() override;
    virtual bool endDocument() override;
    virtual bool startElement(
        const QString& /*namespaceURI*/,
        const QString& /*localName*/,
        const QString& qName,
        const QXmlAttributes& atts ) override;
    virtual bool characters(const QString& ch) override;
    virtual bool endElement(
        const QString& namespaceURI,
        const QString& localName,
        const QString& qName ) override;
    virtual QString errorString() const override;
    virtual bool error( const QXmlParseException& exception ) override;
    virtual bool fatalError( const QXmlParseException& exception ) override;

    std::vector<SocketAddress> moduleUrls(QString moduleName) const;

private:
    enum State
    {
        init,
        readingModules,
        readingModuleEndpoints,
        readingModuleEndpoint
    };

    State m_state;
    QString m_errorDescription;
    std::map<QString, std::vector<SocketAddress>> m_endpoints;
    QString m_currentModule;
};

} // namespace cloud
} // namespace network
} // namespace nx
