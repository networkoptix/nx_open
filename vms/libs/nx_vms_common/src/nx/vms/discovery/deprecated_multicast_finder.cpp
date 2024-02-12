// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "deprecated_multicast_finder.h"

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QMap>
#include <QtNetwork/QNetworkInterface>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <nx/network/nettools.h>
#include <nx/network/socket_global.h>
#include <nx/network/socket.h>
#include <nx/network/system_socket.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::discovery {

namespace {

constexpr int kMaxCacheSize = 64 * 1024;
constexpr unsigned kDefaultPingTimeoutMs = 5'000;
constexpr unsigned kDefaultKeepAliveMultiply = 5;
constexpr unsigned kErrorWaitTimeoutMs = 1'000;
constexpr unsigned kCheckInterfacesTimeoutMs = 60'000;

const QHostAddress kDefaultModuleRevealMulticastGroup = QHostAddress("239.255.11.11");
constexpr quint16 kDefaultModuleRevealMulticastGroupPort = 5007;

constexpr QByteArrayView kRevealRequestStr = "{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }";
constexpr QStringView kModuleInfoStr = u", { seed: \"%1\" }, {peerType: \"%2\"}";

constexpr QStringView kDeprecatedEcId = u"Enterprise Controller";

} // namespace

using namespace nx::network;

//-------------------------------------------------------------------------------------------------
// DeprecatedMulticastFinder::RevealRequest

DeprecatedMulticastFinder::RevealRequest::RevealRequest(
    const nx::Uuid& moduleGuid,
    nx::vms::api::PeerType peerType)
    :
    m_moduleGuid(moduleGuid),
    m_peerType(peerType)
{
}

QByteArray DeprecatedMulticastFinder::RevealRequest::serialize()
{
    QByteArray result{kRevealRequestStr.constData(), kRevealRequestStr.size()};
    QString moduleGuid = m_moduleGuid.toSimpleString();
    result += kModuleInfoStr.arg(moduleGuid, nx::reflect::toString(m_peerType).c_str()).toLatin1();
    return result;
}

bool DeprecatedMulticastFinder::RevealRequest::isValid(
    const quint8* bufStart, const quint8* bufEnd)
{
    if (bufEnd - bufStart < kRevealRequestStr.size())
        return false;

    if (memcmp(bufStart, kRevealRequestStr.data(), kRevealRequestStr.size()) != 0)
        return false;

    return true;
}

//-------------------------------------------------------------------------------------------------
// DeprecatedMulticastFinder::RevealResponse

DeprecatedMulticastFinder::RevealResponse::RevealResponse(
    const nx::vms::api::ModuleInformation& other)
    :
    nx::vms::api::ModuleInformation(other)
{
}

QByteArray DeprecatedMulticastFinder::RevealResponse::serialize()
{
    QVariantMap map;
    map["application"] = type;
    map["seed"] = id.toString();
    map["version"] = version.toString();
    map["customization"] = customization;
    map["brand"] = brand;
    map["realm"] = realm;
    map["systemName"] = systemName;
    map["name"] = name;
    map["sslAllowed"] = sslAllowed;
    map["port"] = port;
    map["protoVersion"] = protoVersion;
    map["runtimeId"] = runtimeId.toString();
    map["flags"] = QString::fromStdString(nx::reflect::toString(serverFlags));
    map["cloudSystemId"] = cloudSystemId;
    map["cloudHost"] = cloudHost;
    map["localSystemId"] = localSystemId.toByteArray();
    return QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
}

bool DeprecatedMulticastFinder::RevealResponse::deserialize(
    const quint8* bufStart,
    const quint8* bufEnd)
{
    while (bufStart < bufEnd && *bufStart != '{')
        bufStart++;

    QByteArray data(reinterpret_cast<const char*>(bufStart), bufEnd - bufStart);

    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    type = map.value("application").toString();
    version = nx::utils::SoftwareVersion(map.value("version").toString());
    customization = map.value("customization").toString();
    brand = map.value("brand").toString();
    realm = map.value("realm").toString();
    systemName = map.value("systemName").toString();
    name = map.value("name").toString();
    id = nx::Uuid::fromStringSafe(map.value("seed").toString());
    sslAllowed = map.value("sslAllowed").toBool();
    port = static_cast<quint16>(map.value("port").toUInt());
    protoVersion = map.value("protoVersion", nx::vms::api::kInitialProtocolVersion).toInt();
    runtimeId = nx::Uuid::fromStringSafe(map.value("runtimeId").toString());
    serverFlags = nx::reflect::fromString<nx::vms::api::ServerFlags>(
        map.value("flags").toString().toStdString(), nx::vms::api::SF_None);
    cloudSystemId = map.value("cloudSystemId").toString();
    cloudHost = map.value("cloudHost").toString();
    localSystemId = nx::Uuid(map.value("localSystemId").toByteArray());
    fixRuntimeId();

    return !type.isEmpty() && !version.isNull();
}

//-------------------------------------------------------------------------------------------------
// DeprecatedMulticastFinder

DeprecatedMulticastFinder::DeprecatedMulticastFinder(
    Options options,
    const QHostAddress &multicastGroupAddress,
    const quint16 multicastGroupPort,
    const unsigned int pingTimeoutMillis,
    const unsigned int keepAliveMultiply)
    :
    m_options(options),
    m_serverSocket(nullptr),
    m_pingTimeoutMillis(pingTimeoutMillis == 0 ? kDefaultPingTimeoutMs : pingTimeoutMillis),
    m_keepAliveMultiply(keepAliveMultiply == 0 ? kDefaultKeepAliveMultiply : keepAliveMultiply),
    m_prevPingClock(0),
    m_checkInterfacesTimeoutMs(kCheckInterfacesTimeoutMs),
    m_lastInterfacesCheckMs(0),
    m_multicastGroupAddress(multicastGroupAddress.isNull()
        ? kDefaultModuleRevealMulticastGroup : multicastGroupAddress),
    m_multicastGroupPort(multicastGroupPort == 0
        ? kDefaultModuleRevealMulticastGroupPort : multicastGroupPort),
    m_cachedResponse(kMaxCacheSize)
{
}

DeprecatedMulticastFinder::~DeprecatedMulticastFinder()
{
    stop();
    clearInterfaces();
}

bool DeprecatedMulticastFinder::isValid() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return !m_clientSockets.empty();
}

bool DeprecatedMulticastFinder::isDisabled(false);

void DeprecatedMulticastFinder::setCheckInterfacesTimeout(unsigned int checkInterfacesTimeoutMs)
{
    m_checkInterfacesTimeoutMs = checkInterfacesTimeoutMs;
}

void DeprecatedMulticastFinder::setMulticastInformation(
    const nx::vms::api::ModuleInformation& information)
{
    //TODO #akolesnikov RevealResponse class is excess here. Should send/receive
    // nx::vms::api::ModuleInformation.
    NX_MUTEX_LOCKER lock(&m_moduleInfoMutex);
    m_serializedModuleInfo = RevealResponse(information).serialize();
}

void DeprecatedMulticastFinder::updateInterfaces()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    std::set<nx::network::HostAddress> addressesToRemove;
    for (const auto& [address, _]: m_clientSockets)
        addressesToRemove.emplace(address);

    /* This function only adds interfaces to the list. */
    for (const QString& addressStr: nx::network::getLocalIpV4AddressList())
    {
        nx::network::HostAddress address(addressStr);
        addressesToRemove.erase(address);

        if (m_clientSockets.count(address))
            continue;

        try
        {
            //if( addressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            auto sock = std::make_unique<nx::network::UDPSocket>(AF_INET);
            sock->bind(nx::network::SocketAddress(address, 0));
            sock->getLocalAddress();    //requesting local address. During this call local port is assigned to socket
            sock->setDestAddr(nx::network::SocketAddress(
                m_multicastGroupAddress.toString().toStdString(), m_multicastGroupPort));

            const auto sockPtr = sock.get();
            m_clientSockets.emplace(address, std::move(sock));
            if (m_serverSocket)
            {
                if (!m_serverSocket->joinGroup(m_multicastGroupAddress, address))
                {
                    NX_WARNING(this, "Unable to join multicast group %1 on interface %2: %3",
                        m_multicastGroupAddress, address);
                }
                else
                {
                    NX_DEBUG(this, "Joined multicast group %1 on interface %2",
                        m_multicastGroupAddress, address);
                }
            }

            if (!m_pollSet.add(sockPtr, aio::etRead, sockPtr))
                NX_ASSERT(false, SystemError::getLastOSErrorText());
            else
                NX_DEBUG(this, "PollSet(%1s): Added %2 socket", m_pollSet.size(), address);
        }
        catch (const std::exception& e)
        {
            NX_ERROR(this, "Failed to create socket on local address %1. %2", address, e.what());
        }
    }

    for (const auto& address: addressesToRemove)
    {
        const auto itr = m_clientSockets.find(address);
        if (!NX_ASSERT(itr != m_clientSockets.end()))
            continue;
        auto socket = std::move(itr->second);
        m_clientSockets.erase(itr);

        m_pollSet.remove(socket.get(), aio::etRead);
        NX_DEBUG(this, "PollSet(%1s): Removed %2 socket from %3",
            m_pollSet.size(), socket->getLocalAddress(), address);

        if (m_serverSocket)
        {
            m_serverSocket->leaveGroup(m_multicastGroupAddress, address);
            NX_DEBUG(this, "Left multicast group %1 on interface %2",
                m_multicastGroupAddress, address);
        }
    }
}

void DeprecatedMulticastFinder::clearInterfaces()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    if (m_serverSocket)
    {
        for (const auto& [address, _]: m_clientSockets)
            m_serverSocket->leaveGroup(m_multicastGroupAddress, address);

        m_pollSet.remove(m_serverSocket.get(), aio::etRead);
        NX_DEBUG(this, "PollSet(%1s): Removed server socket", m_pollSet.size());
        m_serverSocket.reset();
    }

    for (const auto& [address, socket]: m_clientSockets)
    {
        m_pollSet.remove(socket.get(), aio::etRead);
        NX_DEBUG(this, "PollSet(%1s): Removed %2 socket from %3",
            m_pollSet.size(), socket->getLocalAddress(), address);
    }

    m_clientSockets.clear();
    NX_DEBUG(this, "Interfaces are cleared");
}

void DeprecatedMulticastFinder::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_pollSet.interrupt();
}

bool DeprecatedMulticastFinder::processDiscoveryRequest(UDPSocket *udpSocket)
{
    static const size_t READ_BUFFER_SIZE = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[READ_BUFFER_SIZE];

    nx::network::SocketAddress remoteEndpoint;
    int bytesRead = udpSocket->recvFrom(readBuffer, READ_BUFFER_SIZE, &remoteEndpoint);
    if (bytesRead == -1)
    {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_WARNING(this, "Failed to read socket on local address (%1). %2",
            udpSocket->getLocalAddress(), SystemError::toString(prevErrorCode));
        return false;
    }

    if (!RevealRequest::isValid(readBuffer, readBuffer + bytesRead))
    {
        NX_DEBUG(this, "Received invalid request from (%1) on local address %2",
            remoteEndpoint, udpSocket->getLocalAddress());
        return false;
    }

    if (m_options.responseEnabled && !m_options.responseEnabled())
    {
        NX_VERBOSE(this, "Reveal request is ignored from (%1)", remoteEndpoint);
        return true;
    }

    QByteArray serializedModuleInfo;
    {
        NX_MUTEX_LOCKER lock(&m_moduleInfoMutex);
        serializedModuleInfo = m_serializedModuleInfo;
    }

    if (!udpSocket->sendTo(serializedModuleInfo.data(), serializedModuleInfo.size(), remoteEndpoint))
    {
        NX_DEBUG(this, "Can't send response to address (%1)", remoteEndpoint);
        return false;
    };

    NX_VERBOSE(this, "Reveal response is sent to address (%1)", remoteEndpoint);
    return true;
}

DeprecatedMulticastFinder::RevealResponse* DeprecatedMulticastFinder::getCachedValue(
    const quint8* buffer,
    const quint8* bufferEnd)
{
    nx::utils::QnCryptographicHash m_mdctx(nx::utils::QnCryptographicHash::Md5);
    m_mdctx.addData(reinterpret_cast<const char*>(buffer), bufferEnd - buffer);
    QByteArray result = m_mdctx.result();
    RevealResponse* response = m_cachedResponse[result];
    if (!response)
    {
        response = new RevealResponse();
        if (!response->deserialize(buffer, bufferEnd))
        {
            delete response;
            return 0;
        }
        m_cachedResponse.insert(result, response, bufferEnd - buffer);
    }
    return response;
}

bool DeprecatedMulticastFinder::processDiscoveryResponse(UDPSocket *udpSocket)
{
    using nx::vms::common::ServerCompatibilityValidator;

    const size_t readBufferSize = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[readBufferSize];

    nx::network::SocketAddress remoteEndpoint;
    int bytesRead = udpSocket->recvFrom(readBuffer, readBufferSize, &remoteEndpoint);
    if (bytesRead == -1)
    {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_WARNING(this, "Failed to read response on local address (%1). %2",
            udpSocket->getLocalAddress(), SystemError::toString(prevErrorCode));
        return false;
    }

    RevealResponse *response = getCachedValue(readBuffer, readBuffer + bytesRead);
    if (!response)
    {
        NX_DEBUG(this, "Received invalid response from (%1) on local address %2",
            remoteEndpoint, udpSocket->getLocalAddress());
        return false;
    }

    if (response->type != nx::vms::api::ModuleInformation::mediaServerId()
        && response->type != kDeprecatedEcId)
    {
        NX_DEBUG(this, "Ignoring %1 (%2) with id %3 on local address %4",
            response->type, remoteEndpoint, response->id, udpSocket->getLocalAddress());
        return true;
    }

    if (!ServerCompatibilityValidator::isCompatibleCustomization(response->customization))
    {
        NX_DEBUG(this, "Ignoring %1 (%2) with different customization %3 on local address %4",
            response->type, remoteEndpoint, response->customization, udpSocket->getLocalAddress());
        return false;
    }

    if (response->port == 0)
    {
        NX_DEBUG(this, "Ignoring %1 (%2) with zero port on local address %3",
            response->type, remoteEndpoint, udpSocket->getLocalAddress());
        return true;
    }

    NX_VERBOSE(this, "Accepting %1 (%2) with id %3 on local address %4",
        response->type, remoteEndpoint, response->id, udpSocket->getLocalAddress());

    emit responseReceived(
        *response,
        nx::network::SocketAddress(remoteEndpoint.address.toString(), response->port),
        remoteEndpoint.address);
    return true;
}

void DeprecatedMulticastFinder::run()
{
    if (isDisabled)
        return;

    initSystemThreadId();
    NX_DEBUG(this, "Has started");

    QByteArray revealRequest = RevealRequest(
        m_options.peerId,
        qnStaticCommon->localPeerType()).serialize();

    if (m_options.listenAndRespond)
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_serverSocket = std::make_unique<UDPSocket>(AF_INET);
        m_serverSocket->setReuseAddrFlag(true);
        m_serverSocket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, m_multicastGroupPort));
        if (!m_pollSet.add(m_serverSocket.get(), aio::etRead, m_serverSocket.get()))
            NX_ASSERT(false, SystemError::getLastOSErrorText());
        else
            NX_DEBUG(this, nx::format("PollSet(%1s): Added server socket").arg(m_pollSet.size()));
    }

    while (!needToStop())
    {
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            if (m_options.listenAndRespond == false && m_options.multicastCount == 0)
            {
                NX_DEBUG(this, "All work is finished");
                return;
            }
        }

        quint64 currentClock = (quint64) QDateTime::currentMSecsSinceEpoch();
        if (currentClock - m_lastInterfacesCheckMs >= m_checkInterfacesTimeoutMs)
        {
            updateInterfaces();
            m_lastInterfacesCheckMs = currentClock;
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();
        if (currentClock - m_prevPingClock >= m_pingTimeoutMillis)
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            if (m_options.multicastCount != 0)
            {
                if (m_options.multicastCount != Options::kUnlimited)
                    --m_options.multicastCount;

                for (const auto& [_, socket]: m_clientSockets)
                {
                    if (!socket->send(revealRequest.data(), revealRequest.size()))
                    {
                        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                        NX_DEBUG(this, "Failed to send reveal request to %1. %2",
                            socket->getForeignAddress(), SystemError::toString(prevErrorCode));

                        //TODO #akolesnikov if corresponding interface is down, should remove socket from set
                    }
                    else
                    {
                        NX_VERBOSE(this, "Reveal request is sent to %1 from %2",
                            socket->getForeignAddress(), socket->getLocalAddress());
                    }
                }
            }

            m_prevPingClock = currentClock;
        }

        int socketCount = m_pollSet.poll(m_pingTimeoutMillis - (currentClock - m_prevPingClock));
        if (socketCount == 0)
        {
            NX_VERBOSE(this, nx::format("PollSet(%1s): Time out").arg(m_pollSet.size()));
            continue;
        }

        if (socketCount < 0)
        {
            SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            if (prevErrorCode == SystemError::interrupted)
                continue;

            NX_WARNING(this, "Poll failed. %1", SystemError::toString(prevErrorCode));
            msleep(kErrorWaitTimeoutMs);
            continue;
        }

        //currentClock = QDateTime::currentMSecsSinceEpoch();

        NX_VERBOSE(this, "PollSet(%1s): %2 sockets are ready to read",
            m_pollSet.size(), socketCount);

        for (auto it = m_pollSet.begin(); it != m_pollSet.end(); ++it)
        {
            if (!(it.eventType() & aio::etRead))
            {
                NX_ASSERT(false, "Not a read event on socket");
                continue;
            }

            UDPSocket* udpSocket = static_cast<UDPSocket*>(it.userData());

            if (udpSocket == m_serverSocket.get())
                processDiscoveryRequest(udpSocket);
            else
                processDiscoveryResponse(udpSocket);
        }
    }

    clearInterfaces();
    NX_DEBUG(this, "Stopped");
}

} // namespace nx::vms::discovery::discovery
