#include "upnp_resource_searcher.h"
#include "utils/common/sleep.h"
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/deprecated/simple_http_client.h>
#include <nx/network/system_socket.h>

#include <QtXml/QXmlDefaultHandler>
#include "core/resource_management/resource_pool.h"
#include <nx/network/nettools.h>
#include <nx/network/socket.h>

#include <utils/common/app_info.h>


static const QHostAddress groupAddress(QLatin1String("239.255.255.250"));

static const int TCP_TIMEOUT = 3000;
static const int CACHE_TIME_TIME = 1000 * 60 * 5;
static const int GROUP_PORT = 1900;
static const int RECV_BUFFER_SIZE = 1024*1024;

using nx::network::UDPSocket;

// TODO: #mu try to replace with UpnpDeviceDescriptionHandler when upnp camera is avaliable

//!Partial parser for SSDP description xml (UPnP(TM) Device Architecture 1.1, 2.3)
class UpnpResourceDescriptionSaxHandler: public QXmlDefaultHandler
{
    nx::network::upnp::DeviceInfo m_deviceInfo;
    QString m_currentElementName;
public:
    virtual bool startDocument()
    {
        return true;
    }

    virtual bool startElement(const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& /*atts*/)
    {
        m_currentElementName = qName;
        return true;
    }

    virtual bool characters(const QString& ch)
    {
        if (m_currentElementName == QLatin1String("friendlyName"))
            m_deviceInfo.friendlyName = ch;
        else if (m_currentElementName == QLatin1String("manufacturer"))
            m_deviceInfo.manufacturer = ch;
        else if (m_currentElementName == QLatin1String("manufacturerURL"))
            m_deviceInfo.manufacturerUrl = ch;
        else if (m_currentElementName == QLatin1String("modelName"))
            m_deviceInfo.modelName = ch;
        else if (m_currentElementName == QLatin1String("serialNumber"))
            m_deviceInfo.serialNumber = ch;
        else if (m_currentElementName == QLatin1String("UDN"))
            m_deviceInfo.udn = ch;
        else if (m_currentElementName == QLatin1String("presentationURL"))
            m_deviceInfo.presentationUrl = ch;

        return true;
    }

    virtual bool endElement(const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/)
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
    nx::network::upnp::DeviceInfo deviceInfo() const { return m_deviceInfo; }
};




// ====================================================================
QnUpnpResourceSearcher::QnUpnpResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    m_receiveSocket(0)
{
    m_cacheLivetime.restart();

}

QnUpnpResourceSearcher::~QnUpnpResourceSearcher()
{
    for(nx::network::AbstractDatagramSocket* sock: m_socketList)
        delete sock;
    delete m_receiveSocket;
}

nx::network::AbstractDatagramSocket* QnUpnpResourceSearcher::sockByName(const nx::network::QnInterfaceAndAddr& iface)
{
    if (m_receiveSocket == 0)
    {
        UDPSocket* udpSock = new UDPSocket(AF_INET);
        udpSock->setReuseAddrFlag(true);
        udpSock->setRecvBufferSize(RECV_BUFFER_SIZE);
        udpSock->bind( nx::network::SocketAddress( nx::network::HostAddress::anyHost, GROUP_PORT ) );
        for(const auto& iface: nx::network::getAllIPv4Interfaces())
            udpSock->joinGroup(groupAddress.toString(), iface.address.toString());
        m_receiveSocket = udpSock;
    }


    QMap<QString, nx::network::AbstractDatagramSocket*>::iterator it = m_socketList.find(iface.address.toString());
    if (it == m_socketList.end())
    {
        std::unique_ptr<nx::network::AbstractDatagramSocket> sock( nx::network::SocketFactory::createDatagramSocket() );
        QString localAddress = iface.address.toString();

        //if (!sock->bindToInterface(iface))
        if( !sock->bind( nx::network::SocketAddress( iface.address.toString() ) ) ||
            !sock->setMulticastIF( localAddress ) ||
            !sock->setRecvBufferSize( RECV_BUFFER_SIZE ) )
        {
            return 0;
        }



        /*
        if (!sock->joinGroup(groupAddress.toString(), iface.address.toString()))
        {
            delete sock;
            return 0;
        }
        */

        m_socketList.insert(iface.address.toString(), sock.get());

        return sock.release();
    }
    return it.value();
}

QByteArray QnUpnpResourceSearcher::getDeviceDescription(const QByteArray& uuidStr, const QUrl& url)
{
    if (discoveryMode() == DiscoveryMode::fullyEnabled
        && m_cacheLivetime.elapsed() > nx::network::upnp::DeviceSearcher::instance()->cacheTimeout())
    {
        m_cacheLivetime.restart();
        m_deviceXmlCache.clear();
    }

    if (m_deviceXmlCache.contains(uuidStr))
        return m_deviceXmlCache.value(uuidStr);

    CLSimpleHTTPClient http( url.host(), url.port(nx::network::http::DEFAULT_HTTP_PORT), TCP_TIMEOUT, QAuthenticator() );
    CLHttpStatus status = http.doGET( url.path() );
    QByteArray result;
    if (status == CL_HTTP_SUCCESS) {
        http.readAll(result);
    }
    m_deviceXmlCache[uuidStr] = result; // cache anyway for errors as well

    return result;
}

QHostAddress QnUpnpResourceSearcher::findBestIface(const nx::network::HostAddress& host)
{
    QHostAddress oldAddress;
    QHostAddress result;
    for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
    {
        const QHostAddress& newAddress = iface.address;
        if (isNewDiscoveryAddressBetter(host, newAddress, oldAddress))
        {
            oldAddress = newAddress;
            result = newAddress;
        }
    }
    return result;
}

void QnUpnpResourceSearcher::readDeviceXml(
    const QByteArray& uuidStr,
    const QUrl& descritionUrl,
    const nx::network::HostAddress& sender,
    QnResourceList& result )
{
    QByteArray foundDeviceDescription = getDeviceDescription(uuidStr, descritionUrl);
    processDeviceXml(
        foundDeviceDescription,
        nx::network::HostAddress( descritionUrl.host() ),
        sender,
        result );
}

void QnUpnpResourceSearcher::processDeviceXml(
    const QByteArray& foundDeviceDescription,
    const nx::network::HostAddress& host,
    const nx::network::HostAddress& sender,
    QnResourceList& result )
{
    //TODO/IMPL checking Content-Type of received description (MUST be upnp xml description to continue)

    //parsing description xml
    UpnpResourceDescriptionSaxHandler xmlHandler;
    QXmlSimpleReader xmlReader;
    xmlReader.setContentHandler( &xmlHandler );
    xmlReader.setErrorHandler( &xmlHandler );

    QXmlInputSource input;
    input.setData( foundDeviceDescription );
    if( !xmlReader.parse( &input ) )
        return;

    processPacket(findBestIface(sender), host, xmlHandler.deviceInfo(), foundDeviceDescription, QAuthenticator(), result);
}

void QnUpnpResourceSearcher::processSocket(nx::network::AbstractDatagramSocket* socket, QSet<QByteArray>& processedUuid, QnResourceList& result)
{
    while(socket->hasData())
    {
        char buffer[1024*16+1];

        nx::network::SocketAddress sourceEndpoint;
        int readed = socket->recvFrom(buffer, sizeof(buffer), &sourceEndpoint);
        if (readed > 0)
            buffer[readed] = 0;
        QByteArray reply = QByteArray::fromRawData(buffer, readed);

        nx::network::http::Request foundDeviceReply;
        if( !foundDeviceReply.parse( reply ) )
            continue;
        nx::network::http::HttpHeaders::const_iterator locationHeader = foundDeviceReply.headers.find( "LOCATION" );
        if( locationHeader == foundDeviceReply.headers.end() )
            continue;

        nx::network::http::HttpHeaders::const_iterator uuidHeader = foundDeviceReply.headers.find( "USN" );
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

        readDeviceXml(uuidStr, descritionUrl, sourceEndpoint.address, result);
    }
}

QnResourceList QnUpnpResourceSearcher::findResources(void)
{
    QnResourceList result;
    QSet<QByteArray> processedUuid;

    for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
    {
        nx::network::AbstractDatagramSocket* sock = sockByName(iface);
        if (!sock)
            continue;

        QByteArray data;

        data.append(lit("M-SEARCH * HTTP/1.1\r\n"));
        data.append(lit("HOST: "))
            .append(groupAddress.toString())
            .append(lit(":"))
            .append(QString::number(GROUP_PORT))
            .append(lit("\r\n"));
        data.append(lit("MAN: \"ssdp:discover\"\r\n"));
        data.append(lit("MX: 3\r\n"));
        data.append(lit("ST: ssdp:all\r\n\r\n"));
        sock->sendTo(data.data(), data.size(), groupAddress.toString(), GROUP_PORT);

        /*
        data.clear();
        data.append("OPTIONS /ietf/ipp/printer HTTP/1.1\r\n");
        data.append("Host: ").append(sock->getLocalAddress().toLatin1()).append(":").append(QByteArray::number(sock->getLocalPort())).append('\r\n');
        data.append("Request-ID: uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6\r\n");
        sock->sendTo(data.data(), data.size(), groupAddress.toString(), GROUP_PORT);
        */
        processSocket(sock, processedUuid, result);
    }
    if (m_receiveSocket)
        processSocket(m_receiveSocket, processedUuid, result);

    return result;
}



////////////////////////////////////////////////////////////
//// class QnUpnpResourceSearcherAsync
////////////////////////////////////////////////////////////

QnUpnpResourceSearcherAsync::QnUpnpResourceSearcherAsync(QnCommonModule* commonModule) :
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
}

QnResourceList QnUpnpResourceSearcherAsync::findResources()
{
    m_resList.clear();
    nx::network::upnp::DeviceSearcher::instance()->processDiscoveredDevices( this );
    return m_resList;
}

bool QnUpnpResourceSearcherAsync::processPacket(
    const QHostAddress& localInterfaceAddress,
    const nx::network::SocketAddress& discoveredDevAddress,
    const nx::network::upnp::DeviceInfo& devInfo,
    const QByteArray& xmlDevInfo )
{
    const int resListSizeBak = m_resList.size();
    processPacket(
        localInterfaceAddress,
        discoveredDevAddress,
        devInfo,
        xmlDevInfo,
        m_resList );
    return resListSizeBak > m_resList.size();   //device recognized, no need to process this upnp data futher
}
