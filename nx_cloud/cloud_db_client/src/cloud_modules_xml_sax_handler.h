/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_CLOUD_MODULES_XML_SAX_HANDLER_H
#define NX_CDB_CL_CLOUD_MODULES_XML_SAX_HANDLER_H

#include <list>
#include <map>

#include <QtXml/QXmlDefaultHandler>

#include <utils/network/socket_common.h>


namespace nx {
namespace cdb {
namespace cl {


class CloudModulesXmlHandler
:
    public QXmlDefaultHandler
{
public:
    CloudModulesXmlHandler();
    virtual ~CloudModulesXmlHandler();

    virtual bool startDocument() override;
    virtual bool endDocument() override;
    virtual bool startElement(
        const QString& /*namespaceURI*/,
        const QString& /*localName*/,
        const QString& qName,
        const QXmlAttributes& atts ) override;
    virtual bool characters(const QString& ch);
    virtual bool endElement(
        const QString& namespaceURI,
        const QString& localName,
        const QString& qName ) override;
    virtual QString errorString() const override;
    virtual bool error( const QXmlParseException& exception ) override;
    virtual bool fatalError( const QXmlParseException& exception ) override;

    std::list<SocketAddress> moduleUrls(QString moduleName) const;

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
    std::map<QString, std::list<SocketAddress>> m_endpoints;
    QString m_currentModule;
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_CLOUD_MODULES_XML_SAX_HANDLER_H
