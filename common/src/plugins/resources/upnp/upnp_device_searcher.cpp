
#include "upnp_device_searcher.h"

#include <algorithm>
#include <memory>

#include <QtCore/QMutexLocker>
#include <QtXml/QXmlDefaultHandler>

#include <common/common_globals.h>
#include <utils/network/aio/aioservice.h>
#include <utils/network/system_socket.h>

using namespace std;

static const QHostAddress groupAddress(QLatin1String("239.255.255.250"));
static const int GROUP_PORT = 1900;
static const unsigned int MAX_UPNP_RESPONSE_PACKET_SIZE = 512*1024;
static const int XML_DESCRIPTION_LIVE_TIME_MS = 5*60*1000;

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
        else if( m_currentElementName == QLatin1String("presentationURL") ) {
            if (ch.endsWith(lit("/")))
                m_deviceInfo.presentationUrl = ch.left(ch.length()-1);
            else
                m_deviceInfo.presentationUrl = ch;
        }

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

    UpnpDeviceInfo deviceInfo() const { return m_deviceInfo; }
};


////////////////////////////////////////////////////////////
//// class UPNPDeviceSearcher
////////////////////////////////////////////////////////////
static const unsigned int READ_BUF_CAPACITY = 64*1024+1;    //max UDP packet size

UPNPDeviceSearcher::UPNPDeviceSearcher( unsigned int discoverTryTimeoutMS )
:
    m_discoverTryTimeoutMS( discoverTryTimeoutMS == 0 ? DEFAULT_DISCOVER_TRY_TIMEOUT_MS : discoverTryTimeoutMS ),
    m_timerID( 0 ),
    m_readBuf( new char[READ_BUF_CAPACITY] ),
    m_terminated( false )
{
    m_timerID = TimerManager::instance()->addTimer( this, m_discoverTryTimeoutMS );
    m_cacheTimer.start();
}

UPNPDeviceSearcher::~UPNPDeviceSearcher()
{
    pleaseStop();

    delete[] m_readBuf;
    m_readBuf = NULL;
}

void UPNPDeviceSearcher::pleaseStop()
{
    //stopping dispatching discover packets
    {
        QMutexLocker lk( &m_mutex );
        m_terminated = true;
    }
    //m_timerID cannot be changed after m_terminated set to true
    if( m_timerID )
        TimerManager::instance()->joinAndDeleteTimer( m_timerID );

    //since dispatching is stopped, no need to synchronize access to m_socketList
    for( std::map<QString, QSharedPointer<AbstractDatagramSocket> >::const_iterator
        it = m_socketList.begin();
        it != m_socketList.end();
        ++it )
    {
        aio::AIOService::instance()->removeFromWatch( it->second, PollSet::etRead );
    }
    m_socketList.clear();

    //cancelling ongoing http requests
    //NOTE m_httpClients cannot be modified by other threads, since UDP socket processing is over and m_terminated == true
    for( std::map<nx_http::AsyncHttpClient*, DiscoveredDeviceInfo>::iterator
        it = m_httpClients.begin();
        it != m_httpClients.end();
        ++it )
    {
        it->first->terminate();     //this method blocks till event handler returns
        it->first->scheduleForRemoval();
    }
    m_httpClients.clear();
}

void UPNPDeviceSearcher::registerHandler( UPNPSearchHandler* handler )
{
    QMutexLocker lk( &m_mutex );
    std::list<UPNPSearchHandler*>::const_iterator it = std::find( m_handlers.begin(), m_handlers.end(), handler );
    if( it == m_handlers.end() )
        m_handlers.push_back( handler );
}

void UPNPDeviceSearcher::cancelHandlerRegistration( UPNPSearchHandler* handler )
{
    QMutexLocker lk( &m_mutex );
    std::list<UPNPSearchHandler*>::iterator it = std::find( m_handlers.begin(), m_handlers.end(), handler );
    if( it != m_handlers.end() )
        m_handlers.erase( it );
}

void UPNPDeviceSearcher::saveDiscoveredDevicesSnapshot()
{
    QMutexLocker lk( &m_mutex );
    m_discoveredDevicesToProcess = m_discoveredDevices;
    m_discoveredDevices.clear();
}

void UPNPDeviceSearcher::processDiscoveredDevices( UPNPSearchHandler* handlerToUse )
{
    for( std::list<DiscoveredDeviceInfo>::iterator
        it = m_discoveredDevicesToProcess.begin();
        it != m_discoveredDevicesToProcess.end();
         )
    {
        if( handlerToUse )
        {
            if( handlerToUse->processPacket( it->localInterfaceAddress, it->deviceAddress, it->devInfo, it->xmlDevInfo ) )
            {
                m_discoveredDevicesToProcess.erase( it++ );
                continue;
            }
        }
        else
        {
            Q_ASSERT( false );
            //TODO: #ak this needs to be implemented if camera discovery ever becomes truly asynchronous
        }

        ++it;
    }
}

static UPNPDeviceSearcher* UPNPDeviceSearcherInstance = NULL;

void UPNPDeviceSearcher::initGlobalInstance( UPNPDeviceSearcher* _inst )
{
    UPNPDeviceSearcherInstance = _inst;
}

UPNPDeviceSearcher* UPNPDeviceSearcher::instance()
{
    return UPNPDeviceSearcherInstance;
}

void UPNPDeviceSearcher::onTimer( const quint64& /*timerID*/ )
{
    //sending discover packet(s)
    dispatchDiscoverPackets();

    //adding new timer task
    QMutexLocker lk( &m_mutex );
    if( !m_terminated )
        m_timerID = TimerManager::instance()->addTimer( this, m_discoverTryTimeoutMS );
}

void UPNPDeviceSearcher::eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw()
{
    if( eventType == PollSet::etError )
    {
        QSharedPointer<AbstractDatagramSocket> udpSock;
        {
            QMutexLocker lk( &m_mutex );
            //removing socket from m_socketList
            for( map<QString, QSharedPointer<AbstractDatagramSocket> >::iterator
                it = m_socketList.begin();
                it != m_socketList.end();
                ++it )
            {
                if( it->second.data() != sock )
                    continue;
                udpSock = it->second;
                m_socketList.erase( it );
            }
        }
        if( udpSock )
            aio::AIOService::instance()->removeFromWatch( udpSock, PollSet::etRead );
        return;
    }

    nx_http::HttpRequest foundDeviceReply;
    QString remoteHost;

    AbstractDatagramSocket* udpSock = dynamic_cast<AbstractDatagramSocket*>( sock );
    Q_ASSERT( udpSock );

    {
        QMutexLocker lk( &m_mutex );    //locking m_readBuf

        //reading socket and parsing UPnP response packet
        quint16 senderPort;
        int readed = udpSock->recvFrom( m_readBuf, READ_BUF_CAPACITY, remoteHost, senderPort );
        if( readed > 0 )
            m_readBuf[readed] = 0;
        if( !foundDeviceReply.parse( QByteArray::fromRawData(m_readBuf, readed) ) )
            return;
    }

    nx_http::HttpHeaders::const_iterator locationHeader = foundDeviceReply.headers.find( "LOCATION" );
    if( locationHeader == foundDeviceReply.headers.end() )
        return;

    nx_http::HttpHeaders::const_iterator uuidHeader = foundDeviceReply.headers.find( "USN" );
    if( uuidHeader == foundDeviceReply.headers.end() )
        return;

    QByteArray uuidStr = uuidHeader->second;
    if (!uuidStr.startsWith("uuid:"))
        return;
    uuidStr = uuidStr.split(':')[1];

    const QUrl descriptionUrl( QLatin1String(locationHeader->second) );
    uuidStr += descriptionUrl.toString();

    startFetchDeviceXml( uuidStr, descriptionUrl, remoteHost );
}

void UPNPDeviceSearcher::dispatchDiscoverPackets()
{
    foreach( QnInterfaceAndAddr iface, getAllIPv4Interfaces() )
    {
        const QSharedPointer<AbstractDatagramSocket>& sock = getSockByIntf(iface);
        if( !sock )
            continue;

        QByteArray data;

        data.append("M-SEARCH * HTTP/1.1\r\n");
        //data.append("Host: 192.168.0.150:1900\r\n");
        data.append("Host: ").append(sock->getLocalAddress().toString()).append("\r\n");
        data.append("ST:urn:schemas-upnp-org:device:Network Optix Media Server:1\r\n");
        data.append("Man:\"ssdp:discover\"\r\n");
        data.append("MX:3\r\n\r\n");
        sock->sendTo(data.data(), data.size(), groupAddress.toString(), GROUP_PORT);
    }
}

QSharedPointer<AbstractDatagramSocket> UPNPDeviceSearcher::getSockByIntf( const QnInterfaceAndAddr& iface )
{
    const QString& localAddress = iface.address.toString();

    pair<map<QString, QSharedPointer<AbstractDatagramSocket> >::iterator, bool> p;
    {
        QMutexLocker lk( &m_mutex );
        p = m_socketList.insert( make_pair( localAddress, QSharedPointer<AbstractDatagramSocket>() ) );
    }
    if( !p.second )
        return p.first->second;

    //creating new socket
    QSharedPointer<UDPSocket> sock( new UDPSocket() );

    p.first->second = sock;
    if( !sock->setReuseAddrFlag( true ) ||
        !sock->bind( SocketAddress(localAddress, GROUP_PORT) ) ||
        !sock->joinGroup( groupAddress.toString(), iface.address.toString() ) ||
        !sock->setMulticastIF( localAddress ) ||
        !sock->setRecvBufferSize( MAX_UPNP_RESPONSE_PACKET_SIZE ) ||
        !aio::AIOService::instance()->watchSocket( sock, PollSet::etRead, this ) )
    {
        QMutexLocker lk( &m_mutex );
        m_socketList.erase( p.first );
        return QSharedPointer<AbstractDatagramSocket>();
    }

    return sock;
}

void UPNPDeviceSearcher::startFetchDeviceXml( const QByteArray& uuidStr, const QUrl& descriptionUrl, const QString& remoteHost )
{
    DiscoveredDeviceInfo info;
    info.deviceAddress = remoteHost;
    info.localInterfaceAddress = findBestIface( remoteHost );
    info.uuid = uuidStr;
    info.descriptionUrl = descriptionUrl;

    nx_http::AsyncHttpClient* httpClient = NULL;
    //checking, whether new http request is needed
    {
        QMutexLocker lk( &m_mutex );

        //checking upnp description cache
        const UPNPDescriptionCacheItem* cacheItem = findDevDescriptionInCache( uuidStr );
        if( cacheItem )
        {
            //item present in cache, no need to request xml
            info.xmlDevInfo = cacheItem->xmlDevInfo;
            info.devInfo = cacheItem->devInfo;
            m_discoveredDevices.push_back( info );
            return;
        }

        //TODO: #ak linear search is not among fastest ones, known to humanity
        for( std::map<nx_http::AsyncHttpClient*, DiscoveredDeviceInfo>::const_iterator
            it = m_httpClients.begin();
            it != m_httpClients.end();
            ++it )
        {
            if( it->second.uuid == uuidStr || it->second.descriptionUrl == descriptionUrl )
                return; //if there is unfinished request to url descriptionUrl or to device with id uuidStr, then return
        }

        httpClient = new nx_http::AsyncHttpClient();
        m_httpClients.insert( make_pair( httpClient, info ) );
    }

    QObject::connect(
        httpClient, SIGNAL(done(nx_http::AsyncHttpClient*)),
        this, SLOT(onDeviceDescriptionXmlRequestDone(nx_http::AsyncHttpClient*)),
        Qt::DirectConnection );
    if( !httpClient->doGet( descriptionUrl ) )
    {
        QObject::disconnect(
            httpClient, SIGNAL(done(nx_http::AsyncHttpClient*)),
            this, SLOT(onDeviceDescriptionXmlRequestDone(nx_http::AsyncHttpClient*)) );
        httpClient->scheduleForRemoval();

        QMutexLocker lk( &m_mutex );
        m_httpClients.erase( httpClient );
        return;
    }
}

void UPNPDeviceSearcher::processDeviceXml(
    const DiscoveredDeviceInfo& devInfo, 
    const QByteArray& foundDeviceDescription )
{
    //parsing description xml
    UpnpDeviceDescriptionSaxHandler xmlHandler;
    QXmlSimpleReader xmlReader;
    xmlReader.setContentHandler( &xmlHandler );
    xmlReader.setErrorHandler( &xmlHandler );

    QXmlInputSource input;
    input.setData( foundDeviceDescription );
    if( !xmlReader.parse( &input ) )
        return;

    DiscoveredDeviceInfo devInfoFull( devInfo );
    devInfoFull.xmlDevInfo = foundDeviceDescription;
    devInfoFull.devInfo = xmlHandler.deviceInfo();

    QMutexLocker lk( &m_mutex );
    m_discoveredDevices.push_back( devInfoFull );
    updateItemInCache( devInfoFull );
}

QHostAddress UPNPDeviceSearcher::findBestIface( const QString& host )
{
    QString oldAddress;
    foreach( QnInterfaceAndAddr iface, getAllIPv4Interfaces() )
    {
        const QString& newAddress = iface.address.toString();
        if( isNewDiscoveryAddressBetter(host, newAddress, oldAddress) )
            oldAddress = newAddress;
    }
    return QHostAddress(oldAddress);
}

const UPNPDeviceSearcher::UPNPDescriptionCacheItem* UPNPDeviceSearcher::findDevDescriptionInCache( const QByteArray& uuid )
{
    std::map<QByteArray, UPNPDescriptionCacheItem>::iterator it = m_upnpDescCache.find( uuid );
    if( it == m_upnpDescCache.end() )
        return NULL;

    if( m_cacheTimer.elapsed() - it->second.creationTimestamp > XML_DESCRIPTION_LIVE_TIME_MS )
    {
        //item has expired
        m_upnpDescCache.erase( it );
        return NULL;
    }

    return &it->second;
}

void UPNPDeviceSearcher::updateItemInCache( const DiscoveredDeviceInfo& devInfo )
{
    UPNPDescriptionCacheItem& cacheItem = m_upnpDescCache[devInfo.uuid];
    cacheItem.devInfo = devInfo.devInfo;
    cacheItem.xmlDevInfo = devInfo.xmlDevInfo;
    cacheItem.creationTimestamp = m_cacheTimer.elapsed();
}

void UPNPDeviceSearcher::onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClient* httpClient )
{
    DiscoveredDeviceInfo* ctx = NULL;
    {
        QMutexLocker lk( &m_mutex );
        std::map<nx_http::AsyncHttpClient*, DiscoveredDeviceInfo>::iterator it = m_httpClients.find( httpClient );
        if (it == m_httpClients.end())
            return;
        //Q_ASSERT( it != m_httpClients.end() );
        ctx = &it->second;
    }


    if( httpClient->response() && httpClient->response()->statusLine.statusCode == nx_http::StatusCode::ok )
    {
        //TODO: #ak check content type. Must be text/xml; charset="utf-8"
        //reading message body
        const nx_http::BufferType& msgBody = httpClient->fetchMessageBodyBuffer();
        processDeviceXml( *ctx, msgBody );
    }

    QMutexLocker lk( &m_mutex );
    if( m_terminated )
        return;
    httpClient->scheduleForRemoval();
    m_httpClients.erase( httpClient );
}
