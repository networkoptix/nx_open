
#include "upnp_device_searcher.h"

#include <algorithm>
#include <memory>

#include <nx/utils/thread/mutex.h>

#include <api/global_settings.h>
#include <common/common_globals.h>
#include <nx/network/system_socket.h>

#include <utils/common/app_info.h>


using namespace std;
using nx::network::UDPSocket;

static const QHostAddress groupAddress(QLatin1String("239.255.255.250"));
static const int GROUP_PORT = 1900;
static const unsigned int MAX_UPNP_RESPONSE_PACKET_SIZE = 512*1024;
static const int XML_DESCRIPTION_LIVE_TIME_MS = 5*60*1000;
static const int PARTIAL_DISCOVERY_XML_DESCRIPTION_LIVE_TIME_MS = 24*60*60*1000;


////////////////////////////////////////////////////////////
//// class UPNPDeviceSearcher
////////////////////////////////////////////////////////////
static const unsigned int READ_BUF_CAPACITY = 64*1024+1;    //max UDP packet size

static UPNPDeviceSearcher* UPNPDeviceSearcherInstance = nullptr;

UPNPDeviceSearcher::UPNPDeviceSearcher( unsigned int discoverTryTimeoutMS )
:
    m_discoverTryTimeoutMS( discoverTryTimeoutMS == 0 ? DEFAULT_DISCOVER_TRY_TIMEOUT_MS : discoverTryTimeoutMS ),
    m_timerID( 0 ),
    m_readBuf( new char[READ_BUF_CAPACITY] ),
    m_terminated( false )
{
    m_timerID = nx::utils::TimerManager::instance()->addTimer(
        this,
        std::chrono::milliseconds(m_discoverTryTimeoutMS));
    m_cacheTimer.start();

    NX_ASSERT(UPNPDeviceSearcherInstance == nullptr);
    UPNPDeviceSearcherInstance = this;
}

UPNPDeviceSearcher::~UPNPDeviceSearcher()
{
    pleaseStop();

    delete[] m_readBuf;
    m_readBuf = NULL;

    UPNPDeviceSearcherInstance = nullptr;
}

void UPNPDeviceSearcher::pleaseStop()
{
    //stopping dispatching discover packets
    {
        QnMutexLocker lk( &m_mutex );
        m_terminated = true;
    }
    //m_timerID cannot be changed after m_terminated set to true
    if( m_timerID )
        nx::utils::TimerManager::instance()->joinAndDeleteTimer( m_timerID );

    //since dispatching is stopped, no need to synchronize access to m_socketList
    for( std::map<QString, SocketReadCtx>::const_iterator
        it = m_socketList.begin();
        it != m_socketList.end();
        ++it )
    {
        it->second.sock->pleaseStopSync();
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

void UPNPDeviceSearcher::registerHandler( UPNPSearchHandler* handler )
{
    QnMutexLocker lk( &m_mutex );
    std::list<UPNPSearchHandler*>::const_iterator it = std::find( m_handlers.begin(), m_handlers.end(), handler );
    if( it == m_handlers.end() )
        m_handlers.push_back( handler );
}

void UPNPDeviceSearcher::cancelHandlerRegistration( UPNPSearchHandler* handler )
{
    QnMutexLocker lk( &m_mutex );
    std::list<UPNPSearchHandler*>::iterator it = std::find( m_handlers.begin(), m_handlers.end(), handler );
    if( it != m_handlers.end() )
        m_handlers.erase( it );
}

void UPNPDeviceSearcher::saveDiscoveredDevicesSnapshot()
{
    QnMutexLocker lk( &m_mutex );
    m_discoveredDevicesToProcess.swap( m_discoveredDevices );
    m_discoveredDevices.clear();
}

void UPNPDeviceSearcher::processDiscoveredDevices( UPNPSearchHandler* handlerToUse )
{
    for( std::map<HostAddress, DiscoveredDeviceInfo>::iterator
        it = m_discoveredDevicesToProcess.begin();
        it != m_discoveredDevicesToProcess.end();
         )
    {
        if( handlerToUse )
        {
            if( handlerToUse->processPacket(
                    it->second.localInterfaceAddress,
                    it->second.deviceAddress,
                    it->second.devInfo,
                    it->second.xmlDevInfo ) )
            {
                m_discoveredDevicesToProcess.erase( it++ );
                continue;
            }
        }
        else
        {
            NX_ASSERT( false );
            //TODO: #ak this needs to be implemented if camera discovery ever becomes truly asynchronous
        }

        ++it;
    }
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
    QnMutexLocker lk( &m_mutex );
    if( !m_terminated )
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            this,
            std::chrono::milliseconds(m_discoverTryTimeoutMS) );
}

void UPNPDeviceSearcher::onSomeBytesRead(
    AbstractCommunicatingSocket* sock,
    SystemError::ErrorCode errorCode,
    nx::Buffer* readBuffer,
    size_t /*bytesRead*/ ) noexcept
{
    if( errorCode )
    {
        std::shared_ptr<AbstractDatagramSocket> udpSock;
        {
            QnMutexLocker lk( &m_mutex );
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
        udpSock->pleaseStopSync();
        return;
    }

    auto SCOPED_GUARD_FUNC = [this, readBuffer, sock]( UPNPDeviceSearcher* ){
        readBuffer->resize( 0 );
        using namespace std::placeholders;
        sock->readSomeAsync( readBuffer, std::bind( &UPNPDeviceSearcher::onSomeBytesRead, this, sock, _1, readBuffer, _2 ) );
    };
    std::unique_ptr<UPNPDeviceSearcher, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

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

void UPNPDeviceSearcher::dispatchDiscoverPackets()
{
    for(const  QnInterfaceAndAddr& iface: getAllIPv4Interfaces() )
    {
        const std::shared_ptr<AbstractDatagramSocket>& sock = getSockByIntf(iface);
        if( !sock )
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
    }
}

bool UPNPDeviceSearcher::isInterfaceListChanged() const
{
    auto ifaces = getAllIPv4Interfaces().toSet();
    bool changed = false;

    if( changed = ifaces != m_interfacesCache)
        m_interfacesCache = ifaces;

    return changed;
}

void UPNPDeviceSearcher::updateReceiveSocket()
{
    using namespace std::placeholders;

    m_receiveBuffer.reserve(READ_BUF_CAPACITY);

    if (m_receiveSocket)
        m_receiveSocket->cancelIOSync(nx::network::aio::etNone);

    auto udpSock = make_shared<UDPSocket>();
    udpSock->setReuseAddrFlag(true);
    udpSock->setRecvBufferSize(MAX_UPNP_RESPONSE_PACKET_SIZE);
    udpSock->bind( SocketAddress( HostAddress::anyHost, GROUP_PORT ) );
    for(const auto iface: getAllIPv4Interfaces())
        udpSock->joinGroup( groupAddress.toString(), iface.address.toString() );
    udpSock->readSomeAsync(
        &m_receiveBuffer,
        std::bind(
            &UPNPDeviceSearcher::onSomeBytesRead,
            this,
            udpSock.get(), _1, &m_receiveBuffer, _2 ) );

    m_receiveSocket = udpSock;
}

std::shared_ptr<AbstractDatagramSocket> UPNPDeviceSearcher::getSockByIntf( const QnInterfaceAndAddr& iface )
{
    using namespace std::placeholders;

    {
        QnMutexLocker lk(&m_mutex);
        if(isInterfaceListChanged())
            updateReceiveSocket();
    }

    const QString& localAddress = iface.address.toString();

    pair<map<QString, SocketReadCtx>::iterator, bool> p;
    {
        QnMutexLocker lk( &m_mutex );
        p = m_socketList.insert( make_pair( localAddress, SocketReadCtx() ) );
    }
    if( !p.second )
        return p.first->second.sock;

    //creating new socket
    std::shared_ptr<UDPSocket> sock( new UDPSocket() );

    p.first->second.sock = sock;
    p.first->second.buf.reserve( READ_BUF_CAPACITY );
    if (!sock->setReuseAddrFlag( true ) ||
        !sock->bind( SocketAddress( localAddress ) ) ||
        !sock->setMulticastIF( localAddress ) ||
        !sock->setRecvBufferSize( MAX_UPNP_RESPONSE_PACKET_SIZE ))
    {
        sock->post(
            std::bind(
                &UPNPDeviceSearcher::onSomeBytesRead, this,
                sock.get(), SystemError::getLastOSErrorCode(), nullptr, 0));
    }
    else
    {
        sock->readSomeAsync(
            &p.first->second.buf,
            std::bind(
                &UPNPDeviceSearcher::onSomeBytesRead, this,
                sock.get(), _1, &p.first->second.buf, _2));
    }

    return sock;
}

void UPNPDeviceSearcher::startFetchDeviceXml(
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
        QnMutexLocker lk( &m_mutex );

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
        for( std::map<nx_http::AsyncHttpClientPtr, DiscoveredDeviceInfo>::const_iterator
            it = m_httpClients.begin();
            it != m_httpClients.end();
            ++it )
        {
            if( it->second.uuid == uuidStr || it->second.descriptionUrl == descriptionUrl )
                return; //if there is unfinished request to url descriptionUrl or to device with id uuidStr, then return
        }

        httpClient = nx_http::AsyncHttpClient::create();
        m_httpClients[httpClient] = std::move(info);
    }

    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &UPNPDeviceSearcher::onDeviceDescriptionXmlRequestDone,
        Qt::DirectConnection );
    httpClient->doGet( descriptionUrl );
}

void UPNPDeviceSearcher::processDeviceXml(
    const DiscoveredDeviceInfo& devInfo, 
    const QByteArray& foundDeviceDescription )
{
    //parsing description xml
    nx_upnp::DeviceDescriptionHandler xmlHandler;
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

    QnMutexLocker lk( &m_mutex );
    m_discoveredDevices.emplace( devInfo.deviceAddress, devInfoFull );
    updateItemInCache( devInfoFull );
}

QHostAddress UPNPDeviceSearcher::findBestIface( const HostAddress& host )
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

int UPNPDeviceSearcher::cacheTimeout()
{
    int xmlDescriptionLiveTimeout = XML_DESCRIPTION_LIVE_TIME_MS;
    QSet<QString> disabledVendorsForAutoSearch = QnGlobalSettings::instance()->disabledVendorsSet();
    if( disabledVendorsForAutoSearch.size() == 1 &&
        disabledVendorsForAutoSearch.contains(lit("all=partial")) )
    {
        xmlDescriptionLiveTimeout = PARTIAL_DISCOVERY_XML_DESCRIPTION_LIVE_TIME_MS;
    }
    return xmlDescriptionLiveTimeout;
}

const UPNPDeviceSearcher::UPNPDescriptionCacheItem* UPNPDeviceSearcher::findDevDescriptionInCache( const QByteArray& uuid )
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

void UPNPDeviceSearcher::updateItemInCache( const DiscoveredDeviceInfo& devInfo )
{
    UPNPDescriptionCacheItem& cacheItem = m_upnpDescCache[devInfo.uuid];
    cacheItem.devInfo = devInfo.devInfo;
    cacheItem.xmlDevInfo = devInfo.xmlDevInfo;
    cacheItem.creationTimestamp = m_cacheTimer.elapsed();
}

void UPNPDeviceSearcher::onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClientPtr httpClient )
{
    DiscoveredDeviceInfo* ctx = NULL;
    {
        QnMutexLocker lk( &m_mutex );
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

    QnMutexLocker lk( &m_mutex );
    if( m_terminated )
        return;
    httpClient->terminate();
    m_httpClients.erase( httpClient );
}
