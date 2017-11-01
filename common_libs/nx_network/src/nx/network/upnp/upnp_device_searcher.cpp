#include "upnp_device_searcher.h"

#include <algorithm>
#include <memory>

#include <QtCore/QDateTime>
#include <QtXml/QXmlDefaultHandler>

#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/app_info.h>
#include <nx/utils/concurrent.h>

using namespace std;
using namespace nx::network;

static const QHostAddress groupAddress(QLatin1String("239.255.255.250"));
static const int GROUP_PORT = 1900;
static const unsigned int MAX_UPNP_RESPONSE_PACKET_SIZE = 512*1024;
static const int XML_DESCRIPTION_LIVE_TIME_MS = 5*60*1000;
static const unsigned int READ_BUF_CAPACITY = 64*1024+1;    //max UDP packet size

namespace nx_upnp {

int DeviceSearcherDefaultSettings::cacheTimeout() const
{
    return XML_DESCRIPTION_LIVE_TIME_MS;
}

bool DeviceSearcherDefaultSettings::isUpnpMulticastEnabled() const
{
    return true;
}


static DeviceSearcher* UPNPDeviceSearcherInstance = nullptr;

const QString DeviceSearcher::DEFAULT_DEVICE_TYPE = lit("%1 Server")
        .arg(nx::utils::AppInfo::organizationName());

DeviceSearcher::DeviceSearcher(
    const AbstractDeviceSearcherSettings& settings,
    unsigned int discoverTryTimeoutMS )
:
    m_settings(settings),
    m_discoverTryTimeoutMS( discoverTryTimeoutMS == 0 ? DEFAULT_DISCOVER_TRY_TIMEOUT_MS : discoverTryTimeoutMS ),
    m_timerID( 0 ),
    m_terminated( false ),
    m_needToUpdateReceiveSocket(false)
{
    m_receiveBuffer.reserve(READ_BUF_CAPACITY);
    {
        QnMutexLocker lk(&m_mutex);
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            this,
            std::chrono::milliseconds(m_discoverTryTimeoutMS));
    }
    m_cacheTimer.start();

    NX_ASSERT(UPNPDeviceSearcherInstance == nullptr);
    UPNPDeviceSearcherInstance = this;
}

DeviceSearcher::~DeviceSearcher()
{
    pleaseStop();

    UPNPDeviceSearcherInstance = nullptr;
}

void DeviceSearcher::pleaseStop()
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

    auto socket = std::move(m_receiveSocket);
    m_receiveSocket.reset();

    if (socket)
        socket->pleaseStopSync();

    //cancelling ongoing http requests
    //NOTE m_httpClients cannot be modified by other threads, since UDP socket processing is over and m_terminated == true
    for( auto it = m_httpClients.begin();
        it != m_httpClients.end();
        ++it )
    {
        it->first->pleaseStopSync();     //this method blocks till event handler returns
    }
    m_httpClients.clear();
    m_handlerGuard.reset();
}

void DeviceSearcher::registerHandler( SearchHandler* handler, const QString& deviceType )
{
    const auto lock = m_handlerGuard->lock();
    NX_ASSERT(lock);

    // check if handler is registered without deviceType
    const auto& allDev = m_handlers[ QString() ];
    if( allDev.find( handler ) != allDev.end() )
        return;

    // try to register for specified deviceType
    const auto itBool = m_handlers[ deviceType ].insert( std::make_pair( handler, (uintptr_t) handler) );
    if( !itBool.second )
        return;

    // if deviceType was not specified, delete all previous registrations with deviceType
    if( deviceType.isEmpty() )
        for( auto& specDev : m_handlers )
            if( !specDev.first.isEmpty() )
                specDev.second.erase( handler );
}

void DeviceSearcher::unregisterHandler( SearchHandler* handler, const QString& deviceType )
{
    const auto lock = m_handlerGuard->lock();
    NX_ASSERT(lock);

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
    if (deviceType.isEmpty())
    {
        for (auto specDev = m_handlers.begin(); specDev != m_handlers.end();)
        {
            if (!specDev->first.isEmpty()
                && specDev->second.erase( handler )
                && !specDev->second.size())
            {
                m_handlers.erase( specDev++ ); // remove deviceType from discovery
            }
            else
            {
                // if no more subscribers left
                ++specDev;
            }
        }
    }
}

void DeviceSearcher::saveDiscoveredDevicesSnapshot()
{
    QnMutexLocker lk( &m_mutex );
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
            NX_ASSERT( false );
            // TODO: #ak this needs to be implemented if camera discovery ever becomes truly asynchronous
        }

        ++it;
    }
}

int DeviceSearcher::cacheTimeout() const
{
    return m_settings.cacheTimeout();
}

DeviceSearcher* DeviceSearcher::instance()
{
    return UPNPDeviceSearcherInstance;
}

void DeviceSearcher::onTimer( const quint64& /*timerID*/ )
{
    dispatchDiscoverPackets();

    QnMutexLocker lk( &m_mutex );
    if( !m_terminated )
    {
        m_timerID = nx::utils::TimerManager::instance()->addTimer(
            this, std::chrono::milliseconds(m_discoverTryTimeoutMS));
    }
}

void DeviceSearcher::onSomeBytesRead(
    AbstractCommunicatingSocket* sock,
    SystemError::ErrorCode errorCode,
    nx::Buffer* readBuffer,
    size_t /*bytesRead*/ )
{
    if( errorCode )
    {
        {
            QnMutexLocker lk( &m_mutex );
            if (m_terminated)
                return;

            if (sock == m_receiveSocket.get())
            {
                m_needToUpdateReceiveSocket = true;
            }
            else
            {
                //removing socket from m_socketList
                for (auto it = m_socketList.begin(); it != m_socketList.end(); ++it )
                {
                    if (it->second.sock.get() == sock)
                    {
                        m_socketList.erase( it );
                        break;
                    }
                }
            }
        }

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

    const nx::utils::Url descriptionUrl( QLatin1String( locationHeader->second ) );
    uuidStr += descriptionUrl.toString();
    if (descriptionUrl.isValid())
        startFetchDeviceXml( uuidStr, descriptionUrl, remoteHost );
}

void DeviceSearcher::dispatchDiscoverPackets()
{
    if (!m_settings.isUpnpMulticastEnabled())
        return;

    for(const  QnInterfaceAndAddr& iface: getAllIPv4Interfaces() )
    {
        const std::shared_ptr<AbstractDatagramSocket>& sock = getSockByIntf(iface);
        if( !sock )
            continue;

        const auto lock = m_handlerGuard->lock();
        NX_ASSERT(lock);
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

bool DeviceSearcher::needToUpdateReceiveSocket() const
{
    auto ifaces = getAllIPv4Interfaces().toSet();
    bool changed = ifaces != m_interfacesCache;

    if (changed)
        m_interfacesCache = ifaces;

    return changed || m_needToUpdateReceiveSocket;
}

nx::utils::AtomicUniquePtr<AbstractDatagramSocket> DeviceSearcher::updateReceiveSocketUnsafe()
{
    auto oldSock = std::move(m_receiveSocket);

    m_receiveSocket.reset(new UDPSocket(AF_INET));

    m_receiveSocket->setNonBlockingMode(true);
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->setRecvBufferSize(MAX_UPNP_RESPONSE_PACKET_SIZE);
    m_receiveSocket->bind( SocketAddress( HostAddress::anyHost, GROUP_PORT ) );

    for(const auto iface: getAllIPv4Interfaces())
        m_receiveSocket->joinGroup( groupAddress.toString(), iface.address.toString() );

    m_needToUpdateReceiveSocket = false;

    return oldSock;
}

std::shared_ptr<AbstractDatagramSocket> DeviceSearcher::getSockByIntf( const QnInterfaceAndAddr& iface )
{

    nx::utils::AtomicUniquePtr<AbstractDatagramSocket> oldSock;
    bool isReceiveSocketUpdated = false;
    {
        QnMutexLocker lock(&m_mutex);
        isReceiveSocketUpdated = needToUpdateReceiveSocket();
        if(isReceiveSocketUpdated)
            oldSock = updateReceiveSocketUnsafe();
    }

    if (oldSock)
        oldSock->pleaseStopSync(true);
    if (isReceiveSocketUpdated)
    {
        QnMutexLocker lock(&m_mutex);
        m_receiveSocket->readSomeAsync(
            &m_receiveBuffer,
            [this, sock = m_receiveSocket.get(), buf = &m_receiveBuffer](
                SystemError::ErrorCode errorCode,
                std::size_t bytesRead)
        {
            onSomeBytesRead(sock, errorCode, buf, bytesRead);
        });
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

    using namespace std::placeholders;

    p.first->second.sock = sock;
    p.first->second.buf.reserve( READ_BUF_CAPACITY );
    if (!sock->setNonBlockingMode( true ) ||
        !sock->setReuseAddrFlag( true ) ||
        !sock->bind( SocketAddress(localAddress) ) ||
        !sock->setMulticastIF( localAddress ) ||
        !sock->setRecvBufferSize( MAX_UPNP_RESPONSE_PACKET_SIZE ))
    {
        sock->post(
            std::bind(
                &DeviceSearcher::onSomeBytesRead, this,
                sock.get(), SystemError::getLastOSErrorCode(), nullptr, 0));
    }
    else
    {
        //TODO #ak WTF? we start async operation with some internal handle and RETURN this socket
        sock->readSomeAsync(
            &p.first->second.buf,
            std::bind(
                &DeviceSearcher::onSomeBytesRead, this,
                sock.get(), _1, &p.first->second.buf, _2));
    }

    return sock;
}

void DeviceSearcher::startFetchDeviceXml(
    const QByteArray& uuidStr,
    const nx::utils::Url& descriptionUrl,
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
            m_discoveredDevices.emplace( remoteHost, info);
            processPacket(std::move(info));
            return;
        }

        // TODO: #ak linear search is not among fastest ones known to humanity
        for( auto
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
        this, &DeviceSearcher::onDeviceDescriptionXmlRequestDone,
        Qt::DirectConnection );
    httpClient->doGet(descriptionUrl);
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

    QnMutexLocker lk( &m_mutex );
    m_discoveredDevices.emplace( devInfo.deviceAddress, devInfoFull );
    updateItemInCache( std::move(devInfoFull) );
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

const DeviceSearcher::UPNPDescriptionCacheItem* DeviceSearcher::findDevDescriptionInCache( const QByteArray& uuid )
{
    std::map<QByteArray, UPNPDescriptionCacheItem>::iterator it = m_upnpDescCache.find( uuid );
    if( it == m_upnpDescCache.end() )
        return NULL;

    if( m_cacheTimer.elapsed() - it->second.creationTimestamp > m_settings.cacheTimeout() )
    {
        //item has expired
        m_upnpDescCache.erase( it );
        return NULL;
    }

    return &it->second;
}

void DeviceSearcher::updateItemInCache(DiscoveredDeviceInfo info)
{
    UPNPDescriptionCacheItem& cacheItem = m_upnpDescCache[info.uuid];
    cacheItem.devInfo = info.devInfo;
    cacheItem.xmlDevInfo = info.xmlDevInfo;
    cacheItem.creationTimestamp = m_cacheTimer.elapsed();
    processPacket(info);
}

void DeviceSearcher::processPacket(DiscoveredDeviceInfo info)
{
    nx::utils::concurrent::run(
        QThreadPool::globalInstance(),
        [ this, info = std::move(info), guard = m_handlerGuard.sharedGuard() ]()
        {
            const auto lock = guard->lock();
            if( !lock )
                return;

            const SocketAddress devAddress(
                info.descriptionUrl.host(),
                info.descriptionUrl.port() );

            std::map< uint, SearchHandler* > sortedHandlers;
            for( const auto& handler : m_handlers[ QString() ] )
                sortedHandlers[ handler.second ] = handler.first;

            for( const auto& handler : m_handlers[ info.devInfo.deviceType ] )
                sortedHandlers[ handler.second ] = handler.first;

            for( const auto& handler : sortedHandlers )
            {
                handler.second->processPacket(
                    info.localInterfaceAddress, devAddress,
                    info.devInfo, info.xmlDevInfo);
            }
        });
}

void DeviceSearcher::onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClientPtr httpClient )
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
        // TODO: #ak check content type. Must be text/xml; charset="utf-8"
        //reading message body
        const nx_http::BufferType& msgBody = httpClient->fetchMessageBodyBuffer();
        processDeviceXml( *ctx, msgBody );
    }

    QnMutexLocker lk( &m_mutex );
    if( m_terminated )
        return;
    httpClient->pleaseStopSync();
    m_httpClients.erase( httpClient );
}

} // namespace nx_upnp
