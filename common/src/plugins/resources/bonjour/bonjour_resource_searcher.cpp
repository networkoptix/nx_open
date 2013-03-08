#include "bonjour_resource_searcher.h"
#include "utils/common/sleep.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/httptypes.h"
#include "utils/network/simple_http_client.h"

#include <QXmlDefaultHandler>
#include "core/resource_managment/resource_pool.h"

extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);

extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QHostAddress groupAddress(QLatin1String("239.255.255.250"));


static const int TCP_TIMEOUT = 3000;
static const int CACHE_TIME_TIME = 1000 * 60 * 5;

//!Partial parser for SSDP descrition xml (UPnP™ Device Architecture 1.1, 2.3)
class UpnpDeviceDescriptionSaxHandler
:
    public QXmlDefaultHandler
{
    BonjurDeviceInfo m_deviceInfo;
    QString m_currentElementName;
public:
    virtual bool startDocument()
    {
        return true;
    }

    virtual bool startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& /*atts*/ )
    {
        m_currentElementName = qName;
        return true;
    }

    virtual bool characters( const QString& ch )
    {
        if( m_currentElementName == QLatin1String("friendlyName") )
            m_deviceInfo.friendlyName = ch;
        else if( m_currentElementName == QLatin1String("manufacturer") )
            m_deviceInfo.manufacturer = ch;
        else if( m_currentElementName == QLatin1String("modelName") )
            m_deviceInfo.modelName = ch;
        else if( m_currentElementName == QLatin1String("serialNumber") )
            m_deviceInfo.serialNumber = ch;
        else if( m_currentElementName == QLatin1String("presentationURL") )
            m_deviceInfo.presentationUrl = ch;

        return true;
    }

    virtual bool endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
    {
        m_currentElementName.clear();
        return true;
    }

    virtual bool endDocument()
    {
        return true;
    }

    /*
    QString friendlyName() const { return m_friendlyName; }
    QString manufacturer() const { return m_manufacturer; }
    QString modelName() const { return m_modelName; }
    QString serialNumber() const { return m_serialNumber; }
    QString presentationUrl() const { return m_presentationUrl; }
    */
    BonjurDeviceInfo deviceInfo() const { return m_deviceInfo; }
};


//====================================================================
QnBonjourResourceSearcher::QnBonjourResourceSearcher()
{
    m_cacheLivetime.restart();
}

QnBonjourResourceSearcher::~QnBonjourResourceSearcher()
{
    foreach(QUdpSocket* sock, m_socketList)
        delete sock;
}

QUdpSocket* QnBonjourResourceSearcher::sockByName(const QnInterfaceAndAddr& iface)
{
    QMap<QString, QUdpSocket*>::iterator it = m_socketList.find(iface.address.toString());
    if (it == m_socketList.end())
    {
        QUdpSocket* sock = new QUdpSocket();
        if (!bindToInterface(*sock, iface,1900, QUdpSocket::ReuseAddressHint))
        {
            delete sock;
            return 0;
        }

        if (!multicastJoinGroup(*sock, groupAddress, iface.address))
        {
            delete sock;
            return 0;
        }

        m_socketList.insert(iface.address.toString(), sock);

        return sock;
    }
    return it.value();
}

QByteArray QnBonjourResourceSearcher::getDeviceDescription(const QByteArray& uuidStr, const QUrl& url)
{
    if (m_cacheLivetime.elapsed() > CACHE_TIME_TIME) {
        m_cacheLivetime.restart();
        m_deviceXmlCache.clear();
    }

    if (m_deviceXmlCache.contains(uuidStr))
        return m_deviceXmlCache.value(uuidStr);

    CLSimpleHTTPClient http( url.host(), url.port(nx_http::DEFAULT_HTTP_PORT), TCP_TIMEOUT, QAuthenticator() );
    CLHttpStatus status = http.doGET( url.path() );
    QByteArray result;
    if (status == CL_HTTP_SUCCESS) {
        http.readAll(result);
        m_deviceXmlCache[uuidStr] = result;
    }

    return result;
}

QnResourceList QnBonjourResourceSearcher::findResources(void)
{
    QnResourceList result;
    QSet<QByteArray> processedUuid;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        QUdpSocket* sock = sockByName(iface);

        QString tmp = iface.address.toString();


        if (!sock)
            continue;

        while(sock->hasPendingDatagrams())
        {
            QByteArray reply;
            reply.resize(sock->pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;
            sock->readDatagram(reply.data(), reply.size(),    &sender, &senderPort);

            nx_http::HttpRequest foundDeviceReply;
            if( !foundDeviceReply.parse( reply ) )
                continue;
            nx_http::HttpHeaders::const_iterator locationHeader = foundDeviceReply.headers.find( "LOCATION" );
            if( locationHeader == foundDeviceReply.headers.end() )
                continue;

            nx_http::HttpHeaders::const_iterator uuidHeader = foundDeviceReply.headers.find( "USN" );
            if( uuidHeader == foundDeviceReply.headers.end() )
                continue;

            QByteArray uuidStr = uuidHeader->second;
            if (!uuidStr.startsWith("uuid:"))
                continue;
            uuidStr = uuidStr.split(':')[1];

            const QUrl descritionUrl( QLatin1String(locationHeader->second) );
            uuidStr += descritionUrl.toString();

            if (processedUuid.contains(uuidStr))
                continue;
            processedUuid << uuidStr;

            QByteArray foundDeviceDescription = getDeviceDescription(uuidStr, descritionUrl);


            //TODO/IMPL checking Content-Type of received description (MUST be upnp xml description to continue)

            //parsing description xml
            UpnpDeviceDescriptionSaxHandler xmlHandler;
            QXmlSimpleReader xmlReader;
            xmlReader.setContentHandler( &xmlHandler );
            xmlReader.setErrorHandler( &xmlHandler );

            QXmlInputSource input;
            input.setData( foundDeviceDescription );
            if( !xmlReader.parse( &input ) )
                continue;

            processPacket(iface.address, descritionUrl.host(), xmlHandler.deviceInfo(), result);
        }
    }

    return result;
}
