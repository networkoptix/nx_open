#include "multiple_address_connector.h"

#include <nx/network/socket_global.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace network {
namespace cloud {

MultipleAddressConnector::MultipleAddressConnector(
    int ipVersion,
    std::deque<AddressEntry> entries)
    :
    m_ipVersion(ipVersion),
    m_entries(std::move(entries)),
    m_timeout(std::chrono::milliseconds::max())
{
    bindToAioThread(getAioThread());
}

void MultipleAddressConnector::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    
    m_timer.bindToAioThread(aioThread);

    for (auto& connection: m_directConnections)
        connection->bindToAioThread(aioThread);

    for (auto& connector: m_cloudConnectors)
        connector->bindToAioThread(aioThread);
}

void MultipleAddressConnector::connectAsync(
    std::chrono::milliseconds timeout,
    StreamSocketAttributes socketAttributes,
    ConnectHandler handler)
{
    using namespace std::placeholders;

    NX_LOGX(lm("Connecting to %1").arg(containerString(m_entries)), cl_logDEBUG2);

    m_timeout = timeout;
    m_handler = std::move(handler);
    m_socketAttributes = std::move(socketAttributes);

    post(
        [this]()
        {
            m_awaitedConnectOperationCount = 0;
            m_directConnections.clear();
            m_cloudConnectors.clear();

            for (const auto& entry: m_entries)
                connectToEntryAsync(entry);

            if (m_awaitedConnectOperationCount == 0)
                return nx::utils::swapAndCall(m_handler, SystemError::invalidData, nullptr);

            if (m_timeout > std::chrono::milliseconds::zero())
                m_timer.start(m_timeout, std::bind(&MultipleAddressConnector::onTimeout, this));
        });
}

void MultipleAddressConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    
    m_timer.pleaseStopSync();
    m_directConnections.clear();
    m_cloudConnectors.clear();
}

std::unique_ptr<AbstractStreamSocket> MultipleAddressConnector::createTcpSocket(int ipVersion)
{
    return std::make_unique<TCPSocket>(ipVersion);
}

void MultipleAddressConnector::connectToEntryAsync(const AddressEntry& dnsEntry)
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

bool MultipleAddressConnector::establishDirectConnection(const SocketAddress& endpoint)
{
    using namespace std::placeholders;

    NX_LOGX(lm("Trying direct connection to %1").str(endpoint), cl_logDEBUG2);

    auto tcpSocket = createTcpSocket(m_ipVersion);
    tcpSocket->bindToAioThread(getAioThread());
    if (!tcpSocket->setNonBlockingMode(true))
        return false; //< TODO: Provide error code?

    auto tpcSocketPtr = tcpSocket.get();
    m_directConnections.push_back(std::move(tcpSocket));

    tpcSocketPtr->connectAsync(
        endpoint,
        std::bind(&MultipleAddressConnector::onDirectConnectDone, this,
            _1, --m_directConnections.end()));
    return true;
}

void MultipleAddressConnector::onDirectConnectDone(
    SystemError::ErrorCode sysErrorCode,
    std::list<std::unique_ptr<AbstractStreamSocket>>::iterator directConnectionIter)
{
    auto tcpSocket = std::move(*directConnectionIter);
    m_directConnections.erase(directConnectionIter);
    if (sysErrorCode != SystemError::noError)
        tcpSocket.reset();
    else
        tcpSocket->cancelIOSync(aio::etNone);

    onConnectDone(sysErrorCode, std::move(tcpSocket));
}

void MultipleAddressConnector::onConnectDone(
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    NX_LOGX(lm("Connection completed with result %1")
        .arg(SystemError::toString(sysErrorCode)), cl_logDEBUG2);

    if (sysErrorCode == SystemError::noError)
        NX_ASSERT(connection->getAioThread() == getAioThread());
    else
        NX_ASSERT(!connection);

    --m_awaitedConnectOperationCount;
    if (m_awaitedConnectOperationCount > 0 && sysErrorCode != SystemError::noError)
    {
        NX_LOGX(lm("Waiting for another %1 connections to complete...")
            .arg(m_awaitedConnectOperationCount), cl_logDEBUG2);
        return; //< Waiting for other operations to finish.
    }
    
    if (connection)
        m_socketAttributes.applyTo(connection.get());

    cleanUpAndReportResult(sysErrorCode, std::move(connection));
}

void MultipleAddressConnector::establishCloudConnection(const AddressEntry& dnsEntry)
{
    using namespace std::placeholders;

    NX_LOGX(lm("Trying cloud connection to %1").str(dnsEntry.host), cl_logDEBUG2);

    auto cloudConnector = std::make_unique<CloudAddressConnector>(
        dnsEntry,
        m_timeout,
        m_socketAttributes);
    cloudConnector->bindToAioThread(getAioThread());
    auto cloudConnectorPtr = cloudConnector.get();
    m_cloudConnectors.push_back(std::move(cloudConnector));

    cloudConnectorPtr->connectAsync(
        std::bind(&MultipleAddressConnector::onCloudConnectDone, this,
            _1, _2, --m_cloudConnectors.end()));
}

void MultipleAddressConnector::onCloudConnectDone(
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> connection,
    std::list<std::unique_ptr<CloudAddressConnector>>::iterator connectorIter)
{
    NX_ASSERT(isInSelfAioThread());

    m_cloudConnectors.erase(connectorIter);

    onConnectDone(sysErrorCode, std::move(connection));
}

void MultipleAddressConnector::cleanUpAndReportResult(
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    if (sysErrorCode == SystemError::noError)
    {
        NX_LOGX(lm("Reporting connect success. Address %1")
            .arg(containerString(m_entries)), cl_logDEBUG2);
    }
    else
    {
        NX_LOGX(lm("Reporting connect failure (%1). Address %2")
            .arg(SystemError::toString(sysErrorCode)).arg(containerString(m_entries)),
            cl_logDEBUG2);
    }

    NX_ASSERT(isInSelfAioThread());

    m_timer.cancelSync();
    m_directConnections.clear();
    m_cloudConnectors.clear();
    m_awaitedConnectOperationCount = 0;

    nx::utils::swapAndCall(m_handler, sysErrorCode, std::move(connection));
}

void MultipleAddressConnector::onTimeout()
{
    cleanUpAndReportResult(SystemError::timedOut, nullptr);
}

} // namespace cloud
} // namespace network
} // namespace nx
