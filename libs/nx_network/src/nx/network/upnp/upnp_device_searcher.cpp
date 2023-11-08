// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "upnp_device_searcher.h"

#include <algorithm>
#include <memory>

#include <QtCore/QDateTime>

#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>

#include <nx/utils/concurrent.h>
#include <nx/utils/serialization/qt_xml_helper.h>

#include <nx/branding.h>

using namespace std;
using namespace nx::network;

namespace nx::network::upnp {

namespace {

static const QHostAddress groupAddress(QString("239.255.255.250"));
static const int kGroupPort = 1900;
static const unsigned int kMaxUpnpResponsePacketSize = 512 * 1024;
static const int kXmlDescriptionLiveTimeMs = 5 * 60 * 1000;
static const unsigned int kReadBufferCapacity = 64 * 1024 + 1; //< max UDP packet size

} // namespace

int DeviceSearcherDefaultSettings::cacheTimeout() const
{
    return kXmlDescriptionLiveTimeMs;
}

const QString DeviceSearcher::kDefaultDeviceType =
    nx::branding::company() + "Server";

DeviceSearcher::DeviceSearcher(
    nx::utils::TimerManager* timerManager,
    std::unique_ptr<AbstractDeviceSearcherSettings> settings,
    bool ignoreUsb0NetworkInterfaceIfOthersExist,
    std::function<bool()> isHttpsForced,
    unsigned int discoverTryTimeoutMS)
    :
    m_settings(std::move(settings)),
    m_ignoreUsb0NetworkInterfaceIfOthersExist(ignoreUsb0NetworkInterfaceIfOthersExist),
    m_isHttpsForced(isHttpsForced),
    m_discoverTryTimeoutMS(discoverTryTimeoutMS),
    m_timerID(0),
    m_terminated(false),
    m_needToUpdateReceiveSocket(false),
    m_timerManager(timerManager)
{
    m_receiveBuffer.reserve(kReadBufferCapacity);
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_timerID = m_timerManager->addTimer(
            this,
            std::chrono::milliseconds(m_discoverTryTimeoutMS));
    }
    m_cacheTimer.start();
}

DeviceSearcher::~DeviceSearcher()
{
    stop();
}

void DeviceSearcher::pleaseStop()
{
}

void DeviceSearcher::stop()
{
    //stopping dispatching discover packets
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        NX_WRITE_LOCKER lock(&m_stoppingLock);
        m_terminated = true;
    }
    //m_timerID cannot be changed after m_terminated set to true
    if (m_timerID)
    {
        m_timerManager->joinAndDeleteTimer(m_timerID);
        m_timerID = 0;
    }

    //since dispatching is stopped, no need to synchronize access to m_socketList
    for (std::map<QString, SocketReadCtx>::const_iterator
        it = m_socketList.begin();
        it != m_socketList.end();
        ++it)
    {
        it->second.sock->pleaseStopSync();
    }
    m_socketList.clear();

    auto socket = std::move(m_receiveSocket);
    m_receiveSocket.reset();

    if (socket)
        socket->pleaseStopSync();

    // Canceling ongoing http requests.
    // NOTE: m_httpClients cannot be modified by other threads, since UDP socket processing is
    // over and m_terminated == true.
    for (auto it = m_httpClients.begin();
        it != m_httpClients.end();
        ++it)
    {
        it->first->pleaseStopSync();     //this method blocks till event handler returns
    }
    m_httpClients.clear();
    m_handlerGuard.reset();
}

void DeviceSearcher::registerHandler(SearchHandler* handler, const QString& deviceType)
{
    const auto lock = m_handlerGuard->lock();
    NX_ASSERT(lock);

    // check if handler is registered without deviceType
    const auto& allDev = m_handlers[QString()];
    if (allDev.find(handler) != allDev.end())
        return;

    // try to register for specified deviceType
    const auto itBool = m_handlers[deviceType].insert(std::make_pair(handler, (uintptr_t)handler));
    if (!itBool.second)
        return;

    // if deviceType was not specified, delete all previous registrations with deviceType
    if (deviceType.isEmpty())
    {
        for (auto& specDev : m_handlers)
        {
            if (!specDev.first.isEmpty())
                specDev.second.erase(handler);
        }
    }
}

void DeviceSearcher::unregisterHandler(SearchHandler* handler, const QString& deviceType)
{
    const auto lock = m_handlerGuard->lock();
    NX_ASSERT(lock);

    // try to unregister for specified deviceType
    auto it = m_handlers.find(deviceType);
    if (it != m_handlers.end())
        if (it->second.erase(handler))
        {
            // remove deviceType from discovery, if no more subscribers left
            if (!it->second.size() && !deviceType.isEmpty())
                m_handlers.erase(it);
            return;
        }

    // remove all registrations if deviceType is not specified
    if (deviceType.isEmpty())
    {
        for (auto specDev = m_handlers.begin(); specDev != m_handlers.end();)
        {
            if (!specDev->first.isEmpty()
                && specDev->second.erase(handler)
                && !specDev->second.size())
            {
                m_handlers.erase(specDev++); // remove deviceType from discovery
            }
            else
            {
                // if no more subscribers left
                ++specDev;
            }
        }
    }
}

int DeviceSearcher::cacheTimeout() const
{
    return m_settings->cacheTimeout();
}

void DeviceSearcher::onTimer(const quint64& /*timerID*/)
{
    dispatchDiscoverPackets();

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!m_terminated)
    {
        m_timerID = m_timerManager->addTimer(
            this, std::chrono::milliseconds(m_discoverTryTimeoutMS));
    }
}

void DeviceSearcher::onSomeBytesRead(
    AbstractCommunicatingSocket* sock,
    SystemError::ErrorCode errorCode,
    nx::Buffer* readBuffer,
    size_t /*bytesRead*/)
{
    if (errorCode)
    {
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            if (m_terminated)
                return;

            if (sock == m_receiveSocket.get())
            {
                m_needToUpdateReceiveSocket = true;
            }
            else
            {
                //removing socket from m_socketList
                for (auto it = m_socketList.begin(); it != m_socketList.end(); ++it)
                {
                    if (it->second.sock.get() == sock)
                    {
                        m_socketList.erase(it);
                        break;
                    }
                }
            }
        }

        return;
    }

    auto SCOPED_GUARD_FUNC =
        [this, readBuffer, sock](DeviceSearcher*)
        {
            using namespace std::placeholders;

            readBuffer->resize(0);
            sock->readSomeAsync(
                readBuffer,
                std::bind(&DeviceSearcher::onSomeBytesRead, this, sock, _1, readBuffer, _2));
        };
    std::unique_ptr<DeviceSearcher, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD(this, SCOPED_GUARD_FUNC);

    HostAddress remoteHost;
    nx::network::http::Request foundDeviceReply;
    {
        AbstractDatagramSocket* udpSock = static_cast<AbstractDatagramSocket*>(sock);
        //reading socket and parsing UPnP response packet
        remoteHost = udpSock->lastDatagramSourceAddress().address;
        if (!foundDeviceReply.parse(*readBuffer))
            return;
    }

    nx::network::http::HttpHeaders::const_iterator locationHeader = foundDeviceReply.headers.find("LOCATION");
    if (locationHeader == foundDeviceReply.headers.end())
        return;

    nx::network::http::HttpHeaders::const_iterator uuidHeader = foundDeviceReply.headers.find("USN");
    if (uuidHeader == foundDeviceReply.headers.end())
        return;

    auto uuidStr = uuidHeader->second;
    if (!nx::utils::startsWith(uuidStr, "uuid:"))
        return;
    uuidStr = std::get<0>(nx::utils::split_n<2>(uuidStr, ':'))[1];

    using namespace nx::network::http;
    nx::utils::Url descriptionUrl(locationHeader->second);
    const bool useHttps = descriptionUrl.port() == DEFAULT_HTTPS_PORT
        || (m_isHttpsForced && m_isHttpsForced());
    if (useHttps)
    {
        descriptionUrl.setScheme(kSecureUrlSchemeName);
        if (descriptionUrl.port() == DEFAULT_HTTP_PORT)
            descriptionUrl.setPort(DEFAULT_HTTPS_PORT);
    }
    uuidStr += descriptionUrl.toStdString();
    if (descriptionUrl.isValid())
        startFetchDeviceXml(QByteArray::fromStdString(uuidStr), descriptionUrl, remoteHost);
}

void DeviceSearcher::dispatchDiscoverPackets()
{
    for (const auto& address: allLocalAddresses(AddressFilter::onlyFirstIpV4))
    {
        const std::shared_ptr<AbstractDatagramSocket>& sock = getSockByIntf(address);
        if (!sock)
            continue;

        const auto lock = m_handlerGuard->lock();
        NX_ASSERT(lock);
        for( const auto& handler : m_handlers )
        {
            if (std::any_of(handler.second.begin(), handler.second.end(),
                [](const std::pair<SearchHandler*, uintptr_t>& value)
                {
                    return value.first->isEnabled();
                }))
            {
                // undefined device type will trigger default discovery
                const auto& deviceType = !handler.first.isEmpty() ?
                        handler.first : kDefaultDeviceType;

                nx::Buffer data;
                data.append("M-SEARCH * HTTP/1.1\r\n");
                data.append("Host: " + sock->getLocalAddress().toString() + "\r\n");
                data.append("ST:" + toUpnpUrn(deviceType, "device").toStdString() + "\r\n");
                data.append("Man:\"sdp:discover\"\r\n");
                data.append("MX:5\r\n\r\n" );
                sock->sendTo(data.data(), data.size(), groupAddress.toString().toStdString(), kGroupPort);
            }
        }
    }
}

bool DeviceSearcher::needToUpdateReceiveSocket() const
{
    const auto interfacesList = getAllIPv4Interfaces(m_ignoreUsb0NetworkInterfaceIfOthersExist);
    const QSet interfaces(interfacesList.begin(), interfacesList.end());

    bool changed = interfaces != m_interfacesCache;

    if (changed)
        m_interfacesCache = interfaces;

    return changed || m_needToUpdateReceiveSocket;
}

nx::utils::AtomicUniquePtr<AbstractDatagramSocket> DeviceSearcher::updateReceiveSocketUnsafe()
{
    auto oldSock = std::move(m_receiveSocket);

    m_receiveSocket.reset(new UDPSocket(AF_INET));

    m_receiveSocket->setNonBlockingMode(true);
    m_receiveSocket->setReuseAddrFlag(true);
    m_receiveSocket->setRecvBufferSize(kMaxUpnpResponsePacketSize);
    m_receiveSocket->bind(SocketAddress(HostAddress::anyHost, kGroupPort));

    for (const auto& address: allLocalAddresses(AddressFilter::onlyFirstIpV4))
        m_receiveSocket->joinGroup(groupAddress.toString().toStdString(), address.toString());

    m_needToUpdateReceiveSocket = false;

    return oldSock;
}

std::shared_ptr<AbstractDatagramSocket> DeviceSearcher::getSockByIntf(
    const nx::network::HostAddress& address)
{

    nx::utils::AtomicUniquePtr<AbstractDatagramSocket> oldSock;
    bool isReceiveSocketUpdated = false;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        isReceiveSocketUpdated = needToUpdateReceiveSocket();
        if (isReceiveSocketUpdated)
            oldSock = updateReceiveSocketUnsafe();
    }

    if (oldSock)
        oldSock->pleaseStopSync();
    if (isReceiveSocketUpdated)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_receiveSocket->readSomeAsync(
            &m_receiveBuffer,
            [this, sock = m_receiveSocket.get(), buf = &m_receiveBuffer](
                SystemError::ErrorCode errorCode,
                std::size_t bytesRead)
            {
                onSomeBytesRead(sock, errorCode, buf, bytesRead);
            });
    }

    const auto& localAddress = address.toString();

    pair<map<QString, SocketReadCtx>::iterator, bool> p;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        p = m_socketList.emplace(QString::fromStdString(localAddress), SocketReadCtx());
    }
    if (!p.second)
        return p.first->second.sock;

    //creating new socket
    std::shared_ptr<UDPSocket> sock(new UDPSocket());

    using namespace std::placeholders;

    p.first->second.sock = sock;
    p.first->second.buf.reserve(kReadBufferCapacity);
    if (!sock->setNonBlockingMode(true) ||
        !sock->setReuseAddrFlag(true) ||
        !sock->bind(SocketAddress(localAddress)) ||
        !sock->setMulticastIF(localAddress) ||
        !sock->setRecvBufferSize(kMaxUpnpResponsePacketSize))
    {
        sock->post(
            std::bind(
                &DeviceSearcher::onSomeBytesRead, this,
                sock.get(), SystemError::getLastOSErrorCode(), nullptr, 0));
    }
    else
    {
        // TODO: #akolesnikov Something very strange. We start async operation with some internal
        // handle and RETURN this socket.
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
    const HostAddress& remoteHost)
{
    DiscoveredDeviceInfo info;
    info.deviceAddress = remoteHost;
    info.localInterfaceAddress = findBestIface(remoteHost);
    info.uuid = uuidStr;
    info.descriptionUrl = descriptionUrl;

    nx::network::http::AsyncHttpClientPtr httpClient;
    //checking, whether new http request is needed
    {
        NX_MUTEX_LOCKER lk(&m_mutex);

        //checking upnp description cache
        const UPNPDescriptionCacheItem* cacheItem = findDevDescriptionInCache(uuidStr);
        if (cacheItem)
        {
            //item present in cache, no need to request xml
            info.xmlDevInfo = cacheItem->xmlDevInfo;
            info.devInfo = cacheItem->devInfo;
            m_discoveredDevices.emplace(remoteHost, info);
            processPacket(std::move(info));
            return;
        }

        // TODO: #akolesnikov linear search is not among fastest ones known to humanity
        for (auto
            it = m_httpClients.begin();
            it != m_httpClients.end();
            ++it)
        {
            if (it->second.uuid == uuidStr || it->second.descriptionUrl == descriptionUrl)
                return; //if there is unfinished request to url descriptionUrl or to device with id uuidStr, then return
        }

        httpClient = http::AsyncHttpClient::create(ssl::kAcceptAnyCertificate);
        m_httpClients[httpClient] = std::move(info);
    }

    QObject::connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &DeviceSearcher::onDeviceDescriptionXmlRequestDone,
        Qt::DirectConnection);
    httpClient->doGet(descriptionUrl);
}

void DeviceSearcher::processDeviceXml(
    const DiscoveredDeviceInfo& devInfo,
    const nx::Buffer& foundDeviceDescription)
{
    //parsing description xml
    DeviceDescriptionHandler xmlHandler;
    QXmlStreamReader reader(toByteArray(foundDeviceDescription));
    if (!nx::utils::parseXml(reader, xmlHandler))
        return;

    DiscoveredDeviceInfo devInfoFull(devInfo);
    devInfoFull.xmlDevInfo = toByteArray(foundDeviceDescription);
    devInfoFull.devInfo = xmlHandler.deviceInfo();

    NX_MUTEX_LOCKER lk(&m_mutex);
    m_discoveredDevices.emplace(devInfo.deviceAddress, devInfoFull);
    updateItemInCache(std::move(devInfoFull));
}

QHostAddress DeviceSearcher::findBestIface(const HostAddress& host)
{
    QHostAddress oldAddress;
    for (const auto& address: allLocalAddresses(AddressFilter::onlyFirstIpV4))
    {
        QHostAddress newAddress(QString::fromStdString(address.toString()));
        if (isNewDiscoveryAddressBetter(host, newAddress, oldAddress))
            oldAddress = newAddress;
    }
    return oldAddress;
}

const DeviceSearcher::UPNPDescriptionCacheItem* DeviceSearcher::findDevDescriptionInCache(const QByteArray& uuid)
{
    std::map<QByteArray, UPNPDescriptionCacheItem>::iterator it = m_upnpDescCache.find(uuid);
    if (it == m_upnpDescCache.end())
        return nullptr;

    if(m_cacheTimer.elapsed() - it->second.creationTimestamp > m_settings->cacheTimeout())
    {
        //item has expired
        m_upnpDescCache.erase(it);
        return nullptr;
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
        [this, info = std::move(info), guard = m_handlerGuard.sharedGuard()]()
        {
            const auto lock = guard->lock();
            if (!lock)
                return;

            NX_READ_LOCKER stopLock(&m_stoppingLock);
            if (m_terminated)
                return;

            const SocketAddress devAddress(
                info.descriptionUrl.host().toStdString(),
                info.descriptionUrl.port());

            std::map< uint, SearchHandler* > sortedHandlers;
            for (const auto& handler : m_handlers[QString()])
                sortedHandlers[handler.second] = handler.first;

            for (const auto& handler : m_handlers[info.devInfo.deviceType])
                sortedHandlers[handler.second] = handler.first;

            for (const auto& handler : sortedHandlers)
            {
                if (handler.second->isEnabled())
                {
                    handler.second->processPacket(
                        info.localInterfaceAddress, devAddress,
                        info.devInfo, info.xmlDevInfo);
                }
            }
        });
}

void DeviceSearcher::onDeviceDescriptionXmlRequestDone(nx::network::http::AsyncHttpClientPtr httpClient)
{
    DiscoveredDeviceInfo ctx;
    bool processRequired = false;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        HttpClientsDict::iterator it = m_httpClients.find(httpClient);
        if (it == m_httpClients.end())
            return;
        processRequired = httpClient->response()
            && httpClient->response()->statusLine.statusCode == nx::network::http::StatusCode::ok;
        if (processRequired)
            ctx = it->second;
    }

    if (processRequired)
    {
        // TODO: #akolesnikov check content type. Must be text/xml; charset="utf-8"
        //reading message body
        const auto& msgBody = httpClient->fetchMessageBodyBuffer();
        processDeviceXml(ctx, msgBody);
    }

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_terminated)
        return;
    httpClient->pleaseStopSync();
    m_httpClients.erase(httpClient);
}

} // namespace nx::network::upnp
