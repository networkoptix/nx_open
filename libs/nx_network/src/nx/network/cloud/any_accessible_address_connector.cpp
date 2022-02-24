// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "any_accessible_address_connector.h"

#include <nx/network/socket_global.h>
#include <nx/utils/move_only_func.h>

namespace nx::network::cloud {

AnyAccessibleAddressConnector::AnyAccessibleAddressConnector(
    int ipVersion,
    std::deque<AddressEntry> entries)
    :
    m_ipVersion(ipVersion),
    m_entries(std::move(entries)),
    m_timeout(std::chrono::milliseconds::max())
{
    bindToAioThread(getAioThread());
}

void AnyAccessibleAddressConnector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);

    for (auto& connection: m_directConnections)
        connection->bindToAioThread(aioThread);

    for (auto& connector: m_cloudConnectors)
        connector->bindToAioThread(aioThread);
}

void AnyAccessibleAddressConnector::connectAsync(
    std::chrono::milliseconds timeout,
    StreamSocketAttributes socketAttributes,
    ConnectHandler handler)
{
    NX_VERBOSE(this, "Connecting to %1 with timeout %2", containerString(m_entries), timeout);

    m_timeout = timeout;
    m_handler = std::move(handler);
    m_socketAttributes = std::move(socketAttributes);

    dispatch(
        [this]()
        {
            m_awaitedConnectOperationCount = 0;
            m_directConnections.clear();
            m_cloudConnectors.clear();

            for (const auto& entry: m_entries)
                connectToEntryAsync(entry);

            if (m_awaitedConnectOperationCount == 0)
            {
                return post(
                    [this]()
                    {
                        cleanUpAndReportResult(SystemError::invalidData, std::nullopt, nullptr);
                    });
            }

            if (m_timeout > std::chrono::milliseconds::zero())
            {
                // NOTE: Connectors support timeouts on their own.
                // This timeout is added just in case of a bug in a connector(s).
                // So, specifying double timeout here so that connectors always
                // have a chance to report and handle timeout themselves.
                m_timer.start(m_timeout * 2, [this]() { onTimeout(); });
            }
        });
}

void AnyAccessibleAddressConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_timer.pleaseStopSync();
    m_directConnections.clear();
    m_cloudConnectors.clear();

    if (m_awaitedConnectOperationCount > 0)
    {
        NX_VERBOSE(this, "Interrupting ongoing %1 connection(s) to %2",
            m_awaitedConnectOperationCount, containerString(m_entries));
    }
}

std::unique_ptr<AbstractStreamSocket> AnyAccessibleAddressConnector::createTcpSocket(int ipVersion)
{
    return std::make_unique<TCPSocket>(ipVersion);
}

void AnyAccessibleAddressConnector::connectToEntryAsync(const AddressEntry& dnsEntry)
{
    using namespace std::placeholders;

    switch (dnsEntry.type)
    {
        case AddressType::direct:
        {
            SocketAddress endpoint;
            endpoint.address = std::move(dnsEntry.host);
            for (const auto& attr: dnsEntry.attributes)
            {
                if (attr.type == AddressAttributeType::port)
                    endpoint.port = static_cast<quint16>(attr.value);
            }

            if (establishDirectConnection(std::move(endpoint)))
                ++m_awaitedConnectOperationCount;
            break;
        }

        case AddressType::cloud:
        case AddressType::unknown:  //< If peer is unknown, trying to establish cloud connect.
        {
            establishCloudConnection(dnsEntry);
            ++m_awaitedConnectOperationCount;
            break;
        }

        default:
            NX_ASSERT(false);
            break;
    }
}

bool AnyAccessibleAddressConnector::establishDirectConnection(const SocketAddress& endpoint)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "Trying direct connection to %1", endpoint);

    auto tcpSocket = createTcpSocket(m_ipVersion);
    tcpSocket->bindToAioThread(getAioThread());
    if (!tcpSocket->setNonBlockingMode(true) || !tcpSocket->setSendTimeout(m_timeout))
    {
        NX_VERBOSE(this, "Failed to configure socket for %1. %2",
            endpoint, SystemError::getLastOSErrorText());
        return false; //< TODO: Provide error code?
    }

    auto tpcSocketPtr = tcpSocket.get();
    m_directConnections.push_back(std::move(tcpSocket));

    tpcSocketPtr->connectAsync(
        endpoint,
        std::bind(&AnyAccessibleAddressConnector::onDirectConnectDone, this,
            _1, --m_directConnections.end()));
    return true;
}

void AnyAccessibleAddressConnector::onDirectConnectDone(
    SystemError::ErrorCode sysErrorCode,
    std::list<std::unique_ptr<AbstractStreamSocket>>::iterator directConnectionIter)
{
    auto tcpSocket = std::move(*directConnectionIter);
    m_directConnections.erase(directConnectionIter);
    if (sysErrorCode != SystemError::noError)
        tcpSocket.reset();
    else
        tcpSocket->cancelIOSync(aio::etNone);

    onConnectDone(sysErrorCode, AddressType::direct, std::nullopt, std::move(tcpSocket));
}

void AnyAccessibleAddressConnector::onConnectDone(
    SystemError::ErrorCode sysErrorCode,
    AddressType addressType,
    std::optional<TunnelAttributes> cloudTunnelAttributes,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    NX_VERBOSE(this, "Connection completed with result %1, type %2",
        SystemError::toString(sysErrorCode), addressType);

    if (sysErrorCode == SystemError::noError)
    {
        NX_ASSERT(connection->getAioThread() == getAioThread());
        if (!cloudTunnelAttributes)
            cloudTunnelAttributes = TunnelAttributes();
        cloudTunnelAttributes->addressType = addressType;
    }
    else
    {
        NX_ASSERT(!connection);
    }

    --m_awaitedConnectOperationCount;
    if (m_awaitedConnectOperationCount > 0 && sysErrorCode != SystemError::noError)
    {
        NX_VERBOSE(this, "Waiting for another %1 connections to complete...",
            m_awaitedConnectOperationCount);
        return; //< Waiting for other operations to finish.
    }

    if (connection)
        m_socketAttributes.applyTo(connection.get());

    cleanUpAndReportResult(
        sysErrorCode,
        std::move(cloudTunnelAttributes),
        std::move(connection));
}

void AnyAccessibleAddressConnector::establishCloudConnection(const AddressEntry& dnsEntry)
{
    using namespace std::placeholders;

    NX_VERBOSE(this, "Trying cloud connection to %1", dnsEntry.host);

    auto cloudConnector = std::make_unique<CloudAddressConnector>(
        dnsEntry,
        m_timeout,
        m_socketAttributes);
    cloudConnector->bindToAioThread(getAioThread());
    auto cloudConnectorPtr = cloudConnector.get();
    m_cloudConnectors.push_back(std::move(cloudConnector));

    cloudConnectorPtr->connectAsync(
        std::bind(&AnyAccessibleAddressConnector::onCloudConnectDone, this,
            _1, _2, _3, --m_cloudConnectors.end()));
}

void AnyAccessibleAddressConnector::onCloudConnectDone(
    SystemError::ErrorCode sysErrorCode,
    TunnelAttributes cloudTunnelAttributes,
    std::unique_ptr<AbstractStreamSocket> connection,
    std::list<std::unique_ptr<CloudAddressConnector>>::iterator connectorIter)
{
    NX_ASSERT(isInSelfAioThread());

    m_cloudConnectors.erase(connectorIter);

    onConnectDone(
        sysErrorCode,
        AddressType::cloud,
        std::move(cloudTunnelAttributes),
        std::move(connection));
}

void AnyAccessibleAddressConnector::cleanUpAndReportResult(
    SystemError::ErrorCode sysErrorCode,
    std::optional<TunnelAttributes> cloudTunnelAttributes,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    if (sysErrorCode == SystemError::noError)
    {
        NX_VERBOSE(this, "Reporting connect success. Address %1", containerString(m_entries));
    }
    else
    {
        NX_VERBOSE(this, "Reporting connect failure (%1). Address %2",
            SystemError::toString(sysErrorCode), containerString(m_entries));
    }

    NX_ASSERT(isInSelfAioThread());

    m_timer.cancelSync();
    m_directConnections.clear();
    m_cloudConnectors.clear();
    m_awaitedConnectOperationCount = 0;

    nx::utils::swapAndCall(
        m_handler,
        sysErrorCode,
        std::move(cloudTunnelAttributes),
        std::move(connection));
}

void AnyAccessibleAddressConnector::onTimeout()
{
    cleanUpAndReportResult(SystemError::timedOut, std::nullopt, nullptr);
}

} // namespace nx::network::cloud
