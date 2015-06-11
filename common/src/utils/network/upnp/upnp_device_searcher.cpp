#include "upnp_device_searcher.h"

#include <algorithm>
#include <memory>

#include <QtCore/QMutexLocker>
#include <QtXml/QXmlDefaultHandler>

#include <api/global_settings.h>
#include <common/common_globals.h>
#include <utils/network/system_socket.h>

#include <utils/common/app_info.h>

#include <QDateTime>

using namespace std;

static const QHostAddress groupAddress(QLatin1String("239.255.255.250"));
static const int GROUP_PORT = 1900;
static const unsigned int MAX_UPNP_RESPONSE_PACKET_SIZE = 512*1024;
static const int XML_DESCRIPTION_LIVE_TIME_MS = 5*60*1000;
static const int PARTIAL_DISCOVERY_XML_DESCRIPTION_LIVE_TIME_MS = 24*60*60*1000;
static const unsigned int READ_BUF_CAPACITY = 64*1024+1;    //max UDP packet size

namespace nx_upnp {

static DeviceSearcher* UPNPDeviceSearcherInstance = nullptr;

const QString DeviceSearcher::DEFAULT_DEVICE_TYPE = lit("%1 Server")
        .arg(QnAppInfo::organizationName());

DeviceSearcher::DeviceSearcher( unsigned int discoverTryTimeoutMS )
:
    m_discoverTryTimeoutMS( discoverTryTimeoutMS == 0 ? DEFAULT_DISCOVER_TRY_TIMEOUT_MS : discoverTryTimeoutMS ),
    m_timerID( 0 ),
    m_readBuf( new char[READ_BUF_CAPACITY] ),
    m_terminated( false )
{
    m_timerID = TimerManager::instance()->addTimer( this, m_discoverTryTimeoutMS );
    m_cacheTimer.start();

    assert(UPNPDeviceSearcherInstance == nullptr);
    UPNPDeviceSearcherInstance = this;
}

DeviceSearcher::~DeviceSearcher()
{
    pleaseStop();

    delete[] m_readBuf;
    m_readBuf = NULL;

    UPNPDeviceSearcherInstance = nullptr;
}

void DeviceSearcher::pleaseStop()
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
    for( std::map<QString, SocketReadCtx>::const_iterator
        it = m_socketList.begin();
        it != m_socketList.end();
        ++it )
    {
        it->second.sock->terminateAsyncIO( true );
    }
    m_socketList.clear();

    //cancelling ongoing http requests
    //NOTE m_httpClients cannot be modified by other threads, since UDP socket processing is over and m_terminated == true
    for( auto it = m_httpClients.begin();
        it != m_httpClients.end();
        ++it )
    {
        it->first->terminate();     //this method blocks till event handler returns
    }
    m_httpClients.clear();
}

void DeviceSearcher::registerHandler( SearchHandler* handler, const QString& deviceType )
{
    QMutexLocker lk( &m_mutex );

    // check if handler is registred without deviceType
    const auto& allDev = m_handlers[ QString() ];
    if( allDev.find( handler ) != allDev.end() )
        return;

    // try to register for specified deviceType
    const auto now = QDateTime::currentDateTimeUtc().toTime_t();
    const auto itBool = m_handlers[ deviceType ].insert( std::make_pair( handler, now ) );
    if( !itBool.second )
        return;

    // if deviceType was not wspecified, delete all previous registrations with deviceType
    if( deviceType.isEmpty() )
        for( auto& specDev : m_handlers )
            if( !specDev.first.isEmpty() )
                specDev.second.erase( handler );
}

void DeviceSearcher::unregisterHandler( SearchHandler* handler, const QString& deviceType )
{
    QMutexLocker lk( &m_mutex );

    // try to unregister for specified deviceType
    auto it = m_handlers.find( deviceType );
    if( it != m_handlers.end() )
        if( it->second.erase( handler ) )
        {
            // remove deviceType from discovery, if no more subscribers left
            if( !it->second.size() && !deviceType.isEmpty() )
                m_handlers.erase( it );
            return;
        }

    // remove all registrations if deviceType is not specified
    if( deviceType.isEmpty() )
        for( auto specDev = m_handlers.begin(); specDev != m_handlers.end(); )
            if( !specDev->first.isEmpty() &&
                    specDev->second.erase( handler ) &&
                    !specDev->second.size() )
                m_handlers.erase( specDev++ ); // remove deviceType from discovery
            else                               // if no more subscribers left
                ++specDev;
}

void DeviceSearcher::saveDiscoveredDevicesSnapshot()
{
    QMutexLocker lk( &m_mutex );
    m_discoveredDevicesToProcess.swap( m_discoveredDevices );
    m_discoveredDevices.clear();
}

void DeviceSearcher::processDiscoveredDevices( SearchHandler* handlerToUse )
{
    for( std::map<HostAddress, DiscoveredDeviceInfo>::iterator
        it = m_discoveredDevicesToProcess.begin();
        it != m_discoveredDevicesToProcess.end();
         )
    {
        if( handlerToUse )
        {
            const auto url = it->second.descriptionUrl;
            if( handlerToUse->processPacket(
                    it->second.localInterfaceAddress,
                    SocketAddress( url.host(), url.port() ),
                    it->second.devInfo,
                    it->second.xmlDevInfo ) )
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

DeviceSearcher* DeviceSearcher::instance()
{
    return UPNPDeviceSearcherInstance;
}

void DeviceSearcher::onTimer( const quint64& /*timerID*/ )
{
    //sending discover packet(s)
    dispatchDiscoverPackets();

    //adding new timer task
    QMutexLocker lk( &m_mutex );
    if( !m_terminated )
        m_timerID = TimerManager::instance()->addTimer( this, m_discoverTryTimeoutMS );
}

void DeviceSearcher::onSomeBytesRead(
    AbstractCommunicatingSocket* sock,
    SystemError::ErrorCode errorCode,
    nx::Buffer* readBuffer,
    size_t /*bytesRead*/ ) noexcept
{
    if( errorCode )
    {
        std::shared_ptr<AbstractDatagramSocket> udpSock;
        {
            QMutexLocker lk( &m_mutex );
            //removing socket from m_socketList
            for( map<QString, SocketReadCtx>::iterator
                it = m_socketList.begin();
                it != m_socketList.end();
                ++it )
            {
                if( it->second.sock.get() == sock )
                {
                    udpSock = std::move(it->second.sock);
                    m_socketList.erase( it );
                    break;
                }
            }
        }
        udpSock->terminateAsyncIO( true );
        return;
    }

    auto SCOPED_GUARD_FUNC = [this, readBuffer, sock]( DeviceSearcher* ){
        readBuffer->resize( 0 );
        using namespace std::placeholders;
        sock->readSomeAsync( readBuffer, std::bind( &DeviceSearcher::onSomeBytesRead, this, sock, _1, readBuffer, _2 ) );
    };
    std::unique_ptr<DeviceSearcher, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

    HostAddress remoteHost;
    nx_http::Request foundDeviceReply;
    {
        AbstractDatagramSocket* udpSock = static_cast<AbstractDatagramSocket*>(sock);
        //reading socket and parsing UPnP response packet
        remoteHost = udpSock->lastDatagramSourceAddress().address;
        if( !foundDeviceReply.parse( *readBuffer ) )
            return;
    }

    nx_http::HttpHeaders::const_iterator locationHeader = foundDeviceReply.headers.find( "LOCATION" );
    if( locationHeader == foundDeviceReply.headers.end() )
        return;

    nx_http::HttpHeaders::const_iterator uuidHeader = foundDeviceReply.headers.find( "USN" );
    if( uuidHeader == foundDeviceReply.headers.end() )
        return;

    QByteArray uuidStr = uuidHeader->second;
    if( !uuidStr.startsWith( "uuid:" ) )
        return;
    uuidStr = uuidStr.split( ':' )[1];

    const QUrl descriptionUrl( QLatin1String( locationHeader->second ) );
    uuidStr += descriptionUrl.toString();

    startFetchDeviceXml( uuidStr, descriptionUrl, remoteHost );
}

void DeviceSearcher::dispatchDiscoverPackets()
{
    for(const  QnInterfaceAndAddr& iface: getAllIPv4Interfaces() )
    {
        const std::shared_ptr<AbstractDatagramSocket>& sock = getSockByIntf(iface);
        if( !sock )
            continue;

        for( const auto& handler : m_handlers )
        {
            // undefined device type will trigger default discovery
            const auto& deviceType = !handler.first.isEmpty() ?
                        handler.first : DEFAULT_DEVICE_TYPE;

            QByteArray data;
            data.append( lit("M-SEARCH * HTTP/1.1\r\n") );
            data.append( lit("Host: %1\r\n").arg( sock->getLocalAddress().toString() ) );
            data.append( lit("ST:%1\r\n").arg( toUpnpUrn(deviceType, lit("device")) ) );
            data.append( lit("Man:\"ssdp:discover\"\r\n") );
            data.append( lit("MX:3\r\n\r\n") );
            sock->sendTo(data.data(), data.size(), groupAddress.toString(), GROUP_PORT);
        }
    }
}

std::shared_ptr<AbstractDatagramSocket> DeviceSearcher::getSockByIntf( const QnInterfaceAndAddr& iface )
{
    const QString& localAddress = iface.address.toString();

    pair<map<QString, SocketReadCtx>::iterator, bool> p;
    {
        QMutexLocker lk( &m_mutex );
        p = m_socketList.insert( make_pair( localAddress, SocketReadCtx() ) );
    }
    if( !p.second )
        return p.first->second.sock;

    //creating new socket
    std::shared_ptr<UDPSocket> sock( new UDPSocket() );

    using namespace std::placeholders;

    p.first->second.sock = sock;
    p.first->second.buf.reserve( READ_BUF_CAPACITY );
    if( !sock->setReuseAddrFlag( true ) ||
        !sock->bind( SocketAddress(localAddress, GROUP_PORT) ) ||
        !sock->joinGroup( groupAddress.toString(), iface.address.toString() ) ||
        !sock->setMulticastIF( localAddress ) ||
        !sock->setRecvBufferSize( MAX_UPNP_RESPONSE_PACKET_SIZE ) ||
        !sock->readSomeAsync( &p.first->second.buf, std::bind( &DeviceSearcher::onSomeBytesRead, this, sock.get(), _1, &p.first->second.buf, _2 ) ) )
    {
        QMutexLocker lk( &m_mutex );
        m_socketList.erase( p.first );
        return std::shared_ptr<AbstractDatagramSocket>();
    }

    return sock;
}

void DeviceSearcher::startFetchDeviceXml(
    const QByteArray& uuidStr,
    const QUrl& descriptionUrl,
    const HostAddress& remoteHost )
{
    DiscoveredDeviceInfo info;
    info.deviceAddress = remoteHost;
    info.localInterfaceAddress = findBestIface( remoteHost );
    info.uuid = uuidStr;
    info.descriptionUrl = descriptionUrl;

    nx_http::AsyncHttpClientPtr httpClient;
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
            m_discoveredDevices.emplace( remoteHost, std::move(info) );
            return;
        }

        //TODO: #ak linear search is not among fastest ones known to humanity
        for( std::map<std::shared_ptr<nx_http::AsyncHttpClient>, DiscoveredDeviceInfo>::const_iterator
            it = m_httpClients.begin();
            it != m_httpClients.end();
            ++it )
        {
            if( it->second.uuid == uuidStr || it->second.descriptionUrl == descriptionUrl )
                return; //if there is unfinished request to url descriptionUrl or to device with id uuidStr, then return
        }

        httpClient = std::make_shared<nx_http::AsyncHttpClient>();
        m_httpClients[httpClient] = std::move(info);
    }

    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &DeviceSearcher::onDeviceDescriptionXmlRequestDone,
        Qt::DirectConnection );
    if( !httpClient->doGet( descriptionUrl ) )
    {
        QObject::disconnect(
            httpClient.get(), &nx_http::AsyncHttpClient::done,
            this, &DeviceSearcher::onDeviceDescriptionXmlRequestDone );

        QMutexLocker lk( &m_mutex );
        httpClient->terminate();
        m_httpClients.erase( httpClient );
        return;
    }
}

void DeviceSearcher::processDeviceXml(
    const DiscoveredDeviceInfo& devInfo,
    const QByteArray& foundDeviceDescription )
{
    //parsing description xml
    DeviceDescriptionHandler xmlHandler;
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
    m_discoveredDevices.emplace( devInfo.deviceAddress, devInfoFull );
    updateItemInCache( devInfoFull );
}

QHostAddress DeviceSearcher::findBestIface( const HostAddress& host )
{
    QHostAddress oldAddress;
    for(const QnInterfaceAndAddr& iface: getAllIPv4Interfaces() )
    {
        const QHostAddress& newAddress = iface.address;
        if( isNewDiscoveryAddressBetter(host, newAddress, oldAddress) )
            oldAddress = newAddress;
    }
    return oldAddress;
}

int DeviceSearcher::cacheTimeout()
{
    if( auto settings = QnGlobalSettings::instance() )
    {
        const auto disabledVendors = settings->disabledVendorsSet();
        if( disabledVendors.size() == 1 && disabledVendors.contains( lit("all=partial") ) )
            return PARTIAL_DISCOVERY_XML_DESCRIPTION_LIVE_TIME_MS;
    }

    return XML_DESCRIPTION_LIVE_TIME_MS;
}

const DeviceSearcher::UPNPDescriptionCacheItem* DeviceSearcher::findDevDescriptionInCache( const QByteArray& uuid )
{
    std::map<QByteArray, UPNPDescriptionCacheItem>::iterator it = m_upnpDescCache.find( uuid );
    if( it == m_upnpDescCache.end() )
        return NULL;

    if( m_cacheTimer.elapsed() - it->second.creationTimestamp > cacheTimeout() )
    {
        //item has expired
        m_upnpDescCache.erase( it );
        return NULL;
    }

    return &it->second;
}

void DeviceSearcher::updateItemInCache( const DiscoveredDeviceInfo& devInfo )
{
    UPNPDescriptionCacheItem& cacheItem = m_upnpDescCache[devInfo.uuid];
    cacheItem.devInfo = devInfo.devInfo;
    cacheItem.xmlDevInfo = devInfo.xmlDevInfo;
    cacheItem.creationTimestamp = m_cacheTimer.elapsed();

    const auto url = devInfo.descriptionUrl;
    processPacket(devInfo.localInterfaceAddress, SocketAddress(url.host(), url.port()),
                  devInfo.devInfo, devInfo.xmlDevInfo);
}


bool DeviceSearcher::processPacket( const QHostAddress& localInterfaceAddress,
                                        const SocketAddress& discoveredDevAddress,
                                        const DeviceInfo& devInfo,
                                        const QByteArray& xmlDevInfo )
{
    std::map< uint, SearchHandler* > sortedHandlers;
    for( const auto handler : m_handlers[ QString() ] )
        sortedHandlers[ handler.second ] = handler.first;

    for( const auto handler : m_handlers[ devInfo.deviceType ] )
        sortedHandlers[ handler.second ] = handler.first;

    bool precessed = true;
    for( const auto& handler : sortedHandlers )
        precessed &= handler.second->processPacket(
            localInterfaceAddress, discoveredDevAddress, devInfo, xmlDevInfo );
    return precessed;
}

void DeviceSearcher::onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClientPtr httpClient )
{
    DiscoveredDeviceInfo* ctx = NULL;
    {
        QMutexLocker lk( &m_mutex );
        HttpClientsDict::iterator it = m_httpClients.find( httpClient );
        if (it == m_httpClients.end())
            return;
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
    httpClient->terminate();
    m_httpClients.erase( httpClient );
}

SearchAutoHandler::SearchAutoHandler(const QString& devType)
{
    DeviceSearcher::instance()->registerHandler( this, devType );
}

SearchAutoHandler::~SearchAutoHandler()
{
    if( auto searcher = DeviceSearcher::instance() )
        searcher->unregisterHandler( this );
}

} // namespace nx_upnp
