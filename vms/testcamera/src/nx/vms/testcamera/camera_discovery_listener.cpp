#include "camera_discovery_listener.h"

#include <memory>
#include <chrono>

#include <nx/kit/utils.h>
#include <nx/utils/url.h>
#include <nx/network/nettools.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/socket_factory.h>
#include <nx/vms/testcamera/test_camera_ini.h>

#include "logger.h"

using nx::network::SocketAddress;
using nx::network::HostAddress;

namespace nx::vms::testcamera {

CameraDiscoveryListener::CameraDiscoveryListener(
    const Logger* logger,
    std::function<QByteArray()> obtainDiscoveryResponseMessageFunc,
    NetworkOptions networkOptions)
    :
    m_logger(logger),
    m_obtainDiscoveryResponseMessageFunc(std::move(obtainDiscoveryResponseMessageFunc)),
    m_networkOptions(std::move(networkOptions))
{
}

CameraDiscoveryListener::~CameraDiscoveryListener()
{
    stop();
}

bool CameraDiscoveryListener::initialize()
{
    for (const auto& addr: m_networkOptions.localInterfacesToListen)
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
                NX_LOGGER_INFO(m_logger, "Will accept discovery messages from IP range (%1, %2).",
                    iface.subNetworkAddress(), iface.broadcastAddress());
            }
        }
    }

    if (m_allowedIpRanges.size() == 0 && m_networkOptions.localInterfacesToListen.size() > 0)
    {
        NX_LOGGER_ERROR(m_logger, "Unable to obtain IP ranges to accept discovery messages.");
        return false;
    }

    return true;
}

bool CameraDiscoveryListener::serverAddressIsAllowed(
    const SocketAddress& serverAddress)
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

/** @return Empty string on socket being closed, or on error, having logged the error message. */
QByteArray CameraDiscoveryListener::receiveDiscoveryMessage(
    QByteArray* buffer,
    SocketAddress* outServerAddress,
    nx::network::AbstractDatagramSocket* discoverySocket) const
{
    const int bytesRead = discoverySocket->recvFrom(
        buffer->data(), buffer->size(), outServerAddress);

    if (bytesRead == 0)
    {
        NX_LOGGER_WARNING(m_logger, "Discovery socket connection closed by the Server.");
        return QByteArray();
    }

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
    const SocketAddress& serverAddress) const
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
        NX_LOGGER_VERBOSE(m_logger, "Sent discovery response message to Server %1: %2",
            serverAddress, nx::kit::utils::toString(response));
    }
}
/** @return Nullopt on error, having logged the error message. */
std::optional<SocketAddress> CameraDiscoveryListener::obtainDiscoverySocketAddress() const
{
    if (m_networkOptions.discoveryPort <= 0 || m_networkOptions.discoveryPort >= 65536)
    {
        NX_LOGGER_ERROR(m_logger, "Invalid discoveryPort in .ini: %1.",
            m_networkOptions.discoveryPort);
        return std::nullopt;
    }

    return SocketAddress(HostAddress::anyHost, m_networkOptions.discoveryPort);
}

/*virtual*/ void CameraDiscoveryListener::run()
{
    const auto error = //< Intended to be called like: return error("...");
        [this](const QString& message) { NX_LOGGER_ERROR(m_logger, message); };

    auto discoverySocket = nx::network::SocketFactory::createDatagramSocket();
    if (!discoverySocket)
        return error("Unable to create discovery socket.");

    const std::optional<SocketAddress> socketAddress = obtainDiscoverySocketAddress();
    if (!socketAddress)
        return;

    if (m_networkOptions.reuseDiscoveryPort)
    {
        NX_LOGGER_DEBUG(m_logger, "Will reuse discovery port.");

        if (!discoverySocket->setReuseAddrFlag(true))
            return error(lm("Unable tor set the reuse address flag on the discovery socket."));

        if (!discoverySocket->setReusePortFlag(true))
            return error(lm("Unable tor set the reuse port flag on the discovery socket."));
    }

    if (!discoverySocket->bind(*socketAddress))
        return error(lm("Unable to bind discovery socket to %1").args(socketAddress));

    const int socketTimeout = ini().discoveryMessageTimeoutMs;
    if (!discoverySocket->setRecvTimeout(socketTimeout))
        return error(lm("Unable to set discovery socket timeout of %1 ms.").args(socketTimeout));

    QByteArray buffer(1024 * 8, '\0');
    SocketAddress serverAddress;

    const QByteArray expectedDiscoveryMessage = ini().discoveryMessage + QByteArray("\n");

    while (!m_needStop)
    {
        const QByteArray discoveryMessage = receiveDiscoveryMessage(
            &buffer, &serverAddress, discoverySocket.get());
        if (discoveryMessage.isEmpty())
            continue;

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
