#include "deprecated_multicast_finder.h"

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkInterface>

#include <common/static_common_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>
#include <nx/utils/std/cpp14.h>

#include <nx/network/socket.h>
#include <nx/network/system_socket.h>

#include <common/common_module.h>
#include <network/connection_validator.h>

#include <network/module_information.h>

#include <utils/common/app_info.h>
#include <nx/utils/cryptographic_hash.h>
#include <api/global_settings.h>
#include <nx/network/socket_global.h>
#include <nx/network/nettools.h>

namespace nx {
namespace vms {
namespace discovery {

static const int MAX_CACHE_SIZE_BYTES = 1024 * 64;

namespace {

const unsigned defaultPingTimeoutMs = 1000 * 5;
const unsigned defaultKeepAliveMultiply = 5;
const unsigned errorWaitTimeoutMs = 1000;
const unsigned checkInterfacesTimeoutMs = 60 * 1000;

const QHostAddress defaultModuleRevealMulticastGroup = QHostAddress(lit("239.255.11.11"));
const quint16 defaultModuleRevealMulticastGroupPort = 5007;


} // anonymous namespace

using namespace nx::network;

const size_t DeprecatedMulticastFinder::Options::kUnlimited = std::numeric_limits<size_t>::max();

DeprecatedMulticastFinder::DeprecatedMulticastFinder(
    QObject* parent,
    Options options,
    const QHostAddress &multicastGroupAddress,
    const quint16 multicastGroupPort,
    const unsigned int pingTimeoutMillis,
    const unsigned int keepAliveMultiply)
    :
    QnLongRunnable(parent),
    QnCommonModuleAware(parent),
    m_options(options),
    m_serverSocket(nullptr),
    m_pingTimeoutMillis(pingTimeoutMillis == 0 ? defaultPingTimeoutMs : pingTimeoutMillis),
    m_keepAliveMultiply(keepAliveMultiply == 0 ? defaultKeepAliveMultiply : keepAliveMultiply),
    m_prevPingClock(0),
    m_checkInterfacesTimeoutMs(checkInterfacesTimeoutMs),
    m_lastInterfacesCheckMs(0),
    m_multicastGroupAddress(multicastGroupAddress.isNull() ? defaultModuleRevealMulticastGroup : multicastGroupAddress),
    m_multicastGroupPort(multicastGroupPort == 0 ? defaultModuleRevealMulticastGroupPort : multicastGroupPort),
    m_cachedResponse(MAX_CACHE_SIZE_BYTES)
{
    connect(commonModule(), &QnCommonModule::moduleInformationChanged, this, &DeprecatedMulticastFinder::at_moduleInformationChanged, Qt::DirectConnection);
}

DeprecatedMulticastFinder::~DeprecatedMulticastFinder()
{
    stop();
    clearInterfaces();
}

bool DeprecatedMulticastFinder::isValid() const
{
    QnMutexLocker lk(&m_mutex);
    return !m_clientSockets.empty();
}

bool DeprecatedMulticastFinder::isDisabled(false);

void DeprecatedMulticastFinder::setCheckInterfacesTimeout(unsigned int checkInterfacesTimeoutMs)
{
    m_checkInterfacesTimeoutMs = checkInterfacesTimeoutMs;
}

void DeprecatedMulticastFinder::updateInterfaces()
{
    QnMutexLocker lk(&m_mutex);

    QList<QHostAddress> addressesToRemove = m_clientSockets.keys();

    /* This function only adds interfaces to the list. */
    for (const QString &addressStr: nx::network::getLocalIpV4AddressList())
    {
        QHostAddress address(addressStr);
        addressesToRemove.removeOne(address);

        if (m_clientSockets.contains(address))
            continue;

        try
        {
            //if( addressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            auto sock = std::make_unique<nx::network::UDPSocket>(AF_INET);
            sock->bind(nx::network::SocketAddress(address.toString(), 0));
            sock->getLocalAddress();    //requesting local address. During this call local port is assigned to socket
            sock->setDestAddr(nx::network::SocketAddress(m_multicastGroupAddress.toString(), m_multicastGroupPort));
            auto it = m_clientSockets.insert(address, sock.release());
            if (m_serverSocket)
            {
                if (!m_serverSocket->joinGroup(m_multicastGroupAddress.toString(), address.toString()))
                {
                    NX_WARNING(this, lm("Unable to join multicast group %1 on interface %2: %3")
                        .args(m_multicastGroupAddress, address));
                }
                else
                {
                    NX_WARNING(this, lm("Joined multicast group %1 on interface %2")
                        .args(m_multicastGroupAddress, address));
                }
            }

            if (!m_pollSet.add(it.value(), aio::etRead, it.value()))
                NX_ASSERT(false, SystemError::getLastOSErrorText());
            else
                NX_DEBUG(this, lm("PollSet(%1s): Added %2 socket").args(m_pollSet.size(), address));
        }
        catch (const std::exception &e)
        {
            NX_ERROR(this, lm("Failed to create socket on local address %1. %2")
                .args(address, e.what()));
        }
    }

    for (const QHostAddress &address : addressesToRemove)
    {
        UDPSocket *socket = m_clientSockets.take(address);
        m_pollSet.remove(socket, aio::etRead);
        NX_DEBUG(this, lm("PollSet(%1s): Removed %2 socket").args(
            m_pollSet.size(), socket->getLocalAddress()));

        if (m_serverSocket)
        {
            m_serverSocket->leaveGroup(m_multicastGroupAddress.toString(), address.toString());
            NX_DEBUG(this, lm("Left multicast group %1 on interface %2").args(
                m_multicastGroupAddress, address));
        }
    }
}

void DeprecatedMulticastFinder::clearInterfaces()
{
    QnMutexLocker lk(&m_mutex);

    if (m_serverSocket)
    {
        for (auto it = m_clientSockets.begin(); it != m_clientSockets.end(); ++it)
            m_serverSocket->leaveGroup(m_multicastGroupAddress.toString(), it.key().toString());

        m_pollSet.remove(m_serverSocket.get(), aio::etRead);
        NX_DEBUG(this, lm("PollSet(%1s): Removed server socket").args(m_pollSet.size()));
        m_serverSocket.reset();
    }

    for (UDPSocket *socket : m_clientSockets)
    {
        m_pollSet.remove(socket, aio::etRead);
        NX_DEBUG(this, lm("PollSet(%1s): Removed %2 socket").args(
            m_pollSet.size(), socket->getLocalAddress()));

        delete socket;
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
        NX_ERROR(this, lm("Failed to read socket on local address (%1). %2").args(
            udpSocket->getLocalAddress(), SystemError::toString(prevErrorCode)));
        return false;
    }

    if (!RevealRequest::isValid(readBuffer, readBuffer + bytesRead))
    {
        NX_DEBUG(this, lm("Received invalid request from (%1) on local address %2").args(
            remoteEndpoint, udpSocket->getLocalAddress()));
        return false;
    }

    //TODO #ak RevealResponse class is excess here. Should send/receive QnModuleInformation
    {
        QnMutexLocker lock(&m_moduleInfoMutex);
        if (m_serializedModuleInfo.isEmpty())
            m_serializedModuleInfo = RevealResponse(commonModule()->moduleInformation()).serialize();
    }
    if (!udpSocket->sendTo(m_serializedModuleInfo.data(), m_serializedModuleInfo.size(), remoteEndpoint))
    {
        NX_DEBUG(this, lm("Can't send response to address (%1)").arg(remoteEndpoint));
        return false;
    };

    NX_VERBOSE(this, lm("Reveal respose is sent to address (%1)").arg(remoteEndpoint));
    return true;
}

void DeprecatedMulticastFinder::at_moduleInformationChanged()
{
    QnMutexLocker lock(&m_moduleInfoMutex);
    m_serializedModuleInfo.clear(); // clear cached value
}

RevealResponse *DeprecatedMulticastFinder::getCachedValue(const quint8* buffer, const quint8* bufferEnd)
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
    const size_t readBufferSize = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[readBufferSize];

    nx::network::SocketAddress remoteEndpoint;
    int bytesRead = udpSocket->recvFrom(readBuffer, readBufferSize, &remoteEndpoint);
    if (bytesRead == -1)
    {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_ERROR(this, lm("Failed to read response on local address (%1). %2").args(
            udpSocket->getLocalAddress(), SystemError::toString(prevErrorCode)));
        return false;
    }

    RevealResponse *response = getCachedValue(readBuffer, readBuffer + bytesRead);
    if (!response)
    {
        NX_DEBUG(this, lm("Received invalid response from (%1) on local address %2").args(
            remoteEndpoint, udpSocket->getLocalAddress()));
        return false;
    }

    if (response->type != QnModuleInformation::nxMediaServerId()
        && response->type != QnModuleInformation::nxECId())
    {
        NX_DEBUG(this, lm("Ignoring %1 (%2) with id %3 on local address %4").args(
            response->type, remoteEndpoint, response->id, udpSocket->getLocalAddress()));
        return true;
    }

    auto connectionResult = QnConnectionValidator::validateConnection(*response);
    if (connectionResult == Qn::IncompatibleInternalConnectionResult)
    {
        NX_DEBUG(this, lm("Ignoring %1 (%2) with different customization %3 on local address %4").args(
            response->type, remoteEndpoint, response->customization, udpSocket->getLocalAddress()));
        return false;
    }

    if (response->port == 0)
    {
        NX_DEBUG(this, lm("Ignoring %1 (%2) with zero port on local address %3").args(
            response->type, remoteEndpoint, udpSocket->getLocalAddress()));
        return true;
    }

    NX_VERBOSE(this, lm("Accepting %1 (%2) with id %3 on local address %4").args(
        response->type, remoteEndpoint, response->id, udpSocket->getLocalAddress()));

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
    NX_DEBUG(this, lit("Has started"));

    QByteArray revealRequest = RevealRequest(
        commonModule()->moduleGUID(),
        qnStaticCommon->localPeerType()).serialize();

    if (m_options.listenAndRespond)
    {
        QnMutexLocker lk(&m_mutex);
        m_serverSocket.reset(new UDPSocket(AF_INET));
        m_serverSocket->setReuseAddrFlag(true);
        m_serverSocket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, m_multicastGroupPort));
        if (!m_pollSet.add(m_serverSocket.get(), aio::etRead, m_serverSocket.get()))
            NX_ASSERT(false, SystemError::getLastOSErrorText());
        else
            NX_DEBUG(this, lm("PollSet(%1s): Added server socket").arg(m_pollSet.size()));
    }

    while (!needToStop())
    {
        {
            QnMutexLocker lk(&m_mutex);
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
            QnMutexLocker lk(&m_mutex);
            if (m_options.multicastCount != 0)
            {
                if (m_options.multicastCount != Options::kUnlimited)
                    --m_options.multicastCount;

                for (UDPSocket *socket : m_clientSockets)
                {
                    if (!socket->send(revealRequest.data(), revealRequest.size()))
                    {
                        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                        NX_DEBUG(this, lm("Failed to send reveal request to %1. %2").args(
                            socket->getForeignAddress(), SystemError::toString(prevErrorCode)));

                        //TODO #ak if corresponding interface is down, should remove socket from set
                    }
                    else
                    {
                        NX_VERBOSE(this, lm("Reveal request is sent to %1 from %2")
                            .args(socket->getForeignAddress(), socket->getLocalAddress()));
                    }
                }
            }

            m_prevPingClock = currentClock;
        }

        if (const auto timeout = SocketGlobals::debugIni().multicastModuleFinderTimeout)
        {
            NX_INFO(this, lm("Avoid using poll, use %1 ms recv timeouts instead").arg(timeout));
            if (m_serverSocket)
            {
                m_serverSocket->setRecvTimeout((unsigned int) timeout);
                processDiscoveryRequest(m_serverSocket.get());
            }

            for (auto& socket: m_clientSockets)
            {
                socket->setRecvTimeout((unsigned int) timeout);
                processDiscoveryResponse(socket);
            }

            continue;
        }

        int socketCount = m_pollSet.poll(m_pingTimeoutMillis - (currentClock - m_prevPingClock));
        if (socketCount == 0)
        {
            NX_VERBOSE(this, lm("PollSet(%1s): Time out").arg(m_pollSet.size()));
            continue;
        }

        if (socketCount < 0)
        {
            SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            if (prevErrorCode == SystemError::interrupted)
                continue;

            NX_ERROR(this, lit("Poll failed. %1").arg(SystemError::toString(prevErrorCode)));
            msleep(errorWaitTimeoutMs);
            continue;
        }

        //currentClock = QDateTime::currentMSecsSinceEpoch();

        NX_VERBOSE(this, lm("PollSet(%1s): %2 sockets are ready to read").args(
            m_pollSet.size(), socketCount));

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
    NX_DEBUG(this, lit("Stopped"));
}

} // namespace discovery
} // namespace vms
} // namespace nx
