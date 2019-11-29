#include "camera_discovery_listener.h"

#include <memory>
#include <chrono>

#include <nx/kit/utils.h>
#include <nx/network/nettools.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/socket_factory.h>
#include <nx/vms/testcamera/test_camera_ini.h>

#include "logger.h"

namespace nx::vms::testcamera {

CameraDiscoveryListener::CameraDiscoveryListener(
    const Logger* logger,
    std::function<QByteArray()> obtainDiscoveryResponseMessageFunc,
    QStringList localInterfacesToListen)
    :
    m_logger(logger),
    m_obtainDiscoveryResponseMessageFunc(std::move(obtainDiscoveryResponseMessageFunc)),
    m_localInterfacesToListen(std::move(localInterfacesToListen))
{
}

CameraDiscoveryListener::~CameraDiscoveryListener()
{
    stop();
}

bool CameraDiscoveryListener::initialize()
{
    for (const auto& addr: m_localInterfacesToListen)
    {
        for (const auto& iface: nx::network::getAllIPv4Interfaces(
            nx::network::InterfaceListPolicy::keepAllAddressesPerInterface,
            /*ignoreLoopback*/ false))
        {
            if (iface.address == QHostAddress(addr))
            {
                IpRangeV4 ipRange;
                ipRange.firstIp = iface.subNetworkAddress().toIPv4Address();
                ipRange.lastIp = iface.broadcastAddress().toIPv4Address();
                m_allowedIpRanges.push_back(ipRange);
                NX_LOGGER_INFO(m_logger, "Listening to discovery messages from IP range (%1, %2).",
                    iface.subNetworkAddress(), iface.broadcastAddress());
            }
        }
    }

    if (m_allowedIpRanges.size() == 0 && m_localInterfacesToListen.size() > 0)
    {
        NX_LOGGER_ERROR(m_logger,
            "Unable to listen to discovery messages on requested IP addresses.");
        return false;
    }

    return true;
}

bool CameraDiscoveryListener::serverAddressIsAllowed(
    const nx::network::SocketAddress& serverAddress)
{
    if (m_allowedIpRanges.empty()) //< All Server IPs are allowed.
        return true;

    const auto addr = serverAddress.address.ipV4();
    if (!addr)
        return false;

    const auto addrPtr = (quint32*) &addr->s_addr;
    const quint32 v4Addr = ntohl(*addrPtr);

    return std::any_of(
        m_allowedIpRanges.begin(),
        m_allowedIpRanges.end(),
        [v4Addr](const IpRangeV4& range)
        {
            return range.contains(v4Addr);
        });
}

/** @return Empty string on error, having logged the error message. */
QByteArray CameraDiscoveryListener::receiveDiscoveryMessage(
    QByteArray* buffer,
    nx::network::SocketAddress* outServerAddress,
    nx::network::AbstractDatagramSocket* discoverySocket) const
{
    const int bytesRead = discoverySocket->recvFrom(
        buffer->data(), buffer->size(), outServerAddress);

    if (bytesRead == 0)
        return QByteArray();

    if (bytesRead < 0)
    {
        static constexpr char msg[] = "Unable to receive discovery message from %1: %2";
        if (ini().logReceivingDiscoveryMessageErrorAsVerbose)
            NX_LOGGER_VERBOSE(m_logger, msg, *outServerAddress, SystemError::getLastOSErrorText());
        else
            NX_LOGGER_ERROR(m_logger, msg, *outServerAddress, SystemError::getLastOSErrorText());

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); //< To avoid rapid repeat.
        return QByteArray();
    }

    return buffer->left(bytesRead);
}

void CameraDiscoveryListener::sendDiscoveryResponseMessage(
    nx::network::AbstractDatagramSocket* discoverySocket,
    const nx::network::SocketAddress& serverAddress) const
{
    const QByteArray response = m_obtainDiscoveryResponseMessageFunc();

    discoverySocket->setDestAddr(serverAddress);
    const int bytesSent = discoverySocket->send(response.data(), response.size());
    if (bytesSent < 0)
    {
        NX_LOGGER_ERROR(m_logger, "Unable to send discovery response message to Server %1: %2",
            serverAddress, SystemError::getLastOSErrorText());
    }
    else if (bytesSent != response.size())
    {
        NX_LOGGER_ERROR(m_logger, "Unable to send discovery response message to Server %1: "
            "Only sent %2 of %3 bytes.", serverAddress, bytesSent, response.size());
    }
    else
    {
        NX_LOGGER_VERBOSE(m_logger, "Sent discovery response message to Server %1", serverAddress);
    }
}

/*virtual*/ void CameraDiscoveryListener::run()
{
    auto discoverySocket = nx::network::SocketFactory::createDatagramSocket();

    discoverySocket->bind(nx::network::SocketAddress(
        nx::network::HostAddress::anyHost, ini().discoveryPort));

    discoverySocket->setRecvTimeout(ini().discoveryMessageTimeoutMs);
    QByteArray buffer(1024 * 8, '\0');
    nx::network::SocketAddress serverAddress;

    const QByteArray expectedDiscoveryMessage =
        ini().discoveryMessage + QByteArray("\n");

    while (!m_needStop)
    {
        const QByteArray discoveryMessage = receiveDiscoveryMessage(
            &buffer, &serverAddress, discoverySocket.get());

        const bool isServerAllowed = serverAddressIsAllowed(serverAddress);

        const bool isDiscoveryMessageExpected =
            discoveryMessage.startsWith(expectedDiscoveryMessage);

        if (isServerAllowed && isDiscoveryMessageExpected)
        {
            NX_LOGGER_INFO(m_logger, "Got discovery message from Server %1", serverAddress);
            sendDiscoveryResponseMessage(discoverySocket.get(), serverAddress);
            continue;
        }

        const QString serverLogPrefix = isServerAllowed ? "" : " not allowed";
        if (isDiscoveryMessageExpected)
        {
            NX_LOGGER_VERBOSE(m_logger, "Got discovery message from%1 Server %2",
                serverLogPrefix, serverAddress);
        }
        else
        {
            NX_LOGGER_VERBOSE(m_logger, "Got unexpected discovery message from%1 Server %2: %3",
                serverLogPrefix, serverAddress, nx::kit::utils::toString(discoveryMessage));
        }
    }
}

} // namespace nx::vms::testcamera
