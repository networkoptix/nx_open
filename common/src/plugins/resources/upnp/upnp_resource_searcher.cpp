#include "upnp_resource_searcher.h"
#include "utils/common/sleep.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/httptypes.h"
#include "utils/network/simple_http_client.h"

#include <QXmlDefaultHandler>
#include "core/resource_managment/resource_pool.h"
#include "utils/network/nettools.h"

extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);

extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QHostAddress groupAddress(QLatin1String("239.255.255.250"));


static const int TCP_TIMEOUT = 3000;
static const int CACHE_TIME_TIME = 1000 * 60 * 5;
static const int GROUP_PORT = 1900;

//!Partial parser for SSDP descrition xml (UPnP� Device Architecture 1.1, 2.3)
class UpnpDeviceDescriptionSaxHandler
:
    public QXmlDefaultHandler
{
    UpnpDeviceInfo m_deviceInfo;
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
    UpnpDeviceInfo deviceInfo() const { return m_deviceInfo; }
};


//====================================================================
QnUpnpResourceSearcher::QnUpnpResourceSearcher():
    m_receiveSocket(0)
{
    m_cacheLivetime.restart();

}

QnUpnpResourceSearcher::~QnUpnpResourceSearcher()
{
    foreach(UDPSocket* sock, m_socketList)
        delete sock;
    delete m_receiveSocket;
}

UDPSocket* QnUpnpResourceSearcher::sockByName(const QnInterfaceAndAddr& iface)
{
    if (m_receiveSocket == 0) {
        m_receiveSocket = new UDPSocket();
        m_receiveSocket->setReuseAddrFlag(true);
        m_receiveSocket->setLocalPort(GROUP_PORT);
    }

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces()) {
        m_receiveSocket->joinGroup(groupAddress.toString(), iface.address.toString());
    }


    QMap<QString, UDPSocket*>::iterator it = m_socketList.find(iface.address.toString());
    if (it == m_socketList.end())
    {
        UDPSocket* sock = new UDPSocket();
        QString localAddress = iface.address.toString();

        if (!sock->bindToInterface(iface))
        {
            delete sock;
            return 0;
        }

        sock->setMulticastIF(localAddress);

        /*
        if (!sock->joinGroup(groupAddress.toString(), iface.address.toString()))
        {
            delete sock;
            return 0;
        }
        */

        sock->setReadBufferSize(1024*512);

        m_socketList.insert(iface.address.toString(), sock);

        return sock;
    }
    return it.value();
}

QByteArray QnUpnpResourceSearcher::getDeviceDescription(const QByteArray& uuidStr, const QUrl& url)
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

QHostAddress QnUpnpResourceSearcher::findBestIface(const QString& host)
{
    QString oldAddress;
    QString result;
    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        QString newAddress = iface.address.toString();
        if (isNewDiscoveryAddressBetter(host, newAddress, oldAddress))
        {
            oldAddress = newAddress;
            result = newAddress;
        }
    }
    return QHostAddress(result);
}

void QnUpnpResourceSearcher::processDeviceXml(const QByteArray& uuidStr, const QUrl& descritionUrl, const QString& sender, QnResourceList& result)
{
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
        return;

    processPacket(findBestIface(sender), descritionUrl.host(), xmlHandler.deviceInfo(), result);
}

void QnUpnpResourceSearcher::processSocket(UDPSocket* socket, QSet<QByteArray>& processedUuid, QnResourceList& result)
{
    while(socket->hasData())
    {

        char buffer[1024*16+1];

        QString sender;
        quint16 senderPort;
        int readed = socket->recvFrom(buffer, sizeof(buffer), sender, senderPort);
        if (readed > 0)
            buffer[readed] = 0;
        QByteArray reply = QByteArray::fromRawData(buffer, readed);

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

        processDeviceXml(uuidStr, descritionUrl, sender, result);

    }
}

QnResourceList QnUpnpResourceSearcher::findResources(void)
{
    QnResourceList result;
    QSet<QByteArray> processedUuid;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        UDPSocket* sock = sockByName(iface);
        if (!sock)
            continue;

        QByteArray data;

        data.append("M-SEARCH * HTTP/1.1\r\n");
        //data.append("Host: 192.168.0.150:1900\r\n");
        data.append("Host: ").append(sock->getLocalAddress().toAscii()).append(":").append(QByteArray::number(sock->getLocalPort())).append("\r\n");
        data.append("ST:urn:schemas-upnp-org:device:Network Optix Media Server:1\r\n");
        data.append("Man:\"ssdp:discover\"\r\n");
        data.append("MX:3\r\n\r\n");
        sock->sendTo(data.data(), data.size(), groupAddress.toString(), GROUP_PORT);

        /*
        data.clear();
        data.append("OPTIONS /ietf/ipp/printer HTTP/1.1\r\n");
        data.append("Host: ").append(sock->getLocalAddress().toAscii()).append(":").append(QByteArray::number(sock->getLocalPort())).append('\r\n');
        data.append("Request-ID: uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6\r\n");
        sock->sendTo(data.data(), data.size(), groupAddress.toString(), GROUP_PORT);
        */
        processSocket(sock, processedUuid, result);
    }
    if (m_receiveSocket)
        processSocket(m_receiveSocket, processedUuid, result);

    return result;
}
