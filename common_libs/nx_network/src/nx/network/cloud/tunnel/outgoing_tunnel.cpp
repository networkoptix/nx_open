/**********************************************************
* Feb 3, 2016
* akolesnikov
***********************************************************/

#include "outgoing_tunnel.h"

#include "udp_hole_punching_connector.h"


namespace nx {
namespace network {
namespace cloud {

OutgoingTunnel::OutgoingTunnel(AddressEntry targetPeerAddress)
:
    Tunnel(targetPeerAddress.host.toString().toLatin1()),
    m_targetPeerAddress(std::move(targetPeerAddress))
{
}

void OutgoingTunnel::pleaseStop(std::function<void()> handler)
{
    //TODO #ak real implementation
    Tunnel::pleaseStop(std::move(handler));
}

void OutgoingTunnel::establishNewConnection(
    boost::optional<std::chrono::milliseconds> timeout,
    SocketAttributes socketAttributes,
    NewConnectionHandler handler)
{
    QnMutexLocker lk(&m_mutex);
    switch(m_state)
    {
        case State::kConnected:
            lk.unlock();
            using namespace std::placeholders;
            m_connection->establishNewConnection(
                timeout,
                std::move(socketAttributes),
                [handler, this](    //TODO #ak #msvc2015 move to lambda
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket,
                    bool tunnelStillValid)
                {
                    onConnectFinished(
                        std::move(handler),
                        errorCode,
                        std::move(socket),
                        tunnelStillValid);
                });
            break;

        case State::kClosed:
            lk.unlock();
            m_aioThreadBinder.post(
                std::bind(handler, SystemError::connectionReset, nullptr));
            break;

        case State::kInit:
            startAsyncTunnelConnect(&lk);

        case State::kConnecting:
        {
            //saving handler for later use
            const auto timeoutTimePoint =
                timeout
                ? std::chrono::steady_clock::now() + timeout.get()
                : std::chrono::steady_clock::time_point::max();
            ConnectionRequestData data;
            data.socketAttributes = std::move(socketAttributes);
            data.timeout = std::move(timeout);
            data.handler = std::move(handler);
            m_connectHandlers.emplace(timeoutTimePoint, std::move(data));
            break;
        }

        default:
            Q_ASSERT_X(
                false,
                Q_FUNC_INFO,
                lm("Unexpected state %1").
                    arg(stateToString(m_state)).toStdString().c_str());
            break;
    }
}

void OutgoingTunnel::onConnectFinished(
    NewConnectionHandler handler,
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractStreamSocket> socket,
    bool stillValid)
{
    handler(code, std::move(socket));
    if (!stillValid)
        onTunnelClosed();
}

void OutgoingTunnel::onTunnelClosed()
{
    m_aioThreadBinder.dispatch(
        [this]()
        {
            std::function<void(State)> tunnelClosedHandler;
            {
                QnMutexLocker lk(&m_mutex);
                tunnelClosedHandler = std::move(m_stateHandler);
                m_state = State::kClosed;
            }
            tunnelClosedHandler(State::kClosed);
        });
}

void OutgoingTunnel::startAsyncTunnelConnect(QnMutexLockerBase* const /*locker*/)
{
    m_state = State::kConnecting;
    m_connectors = createAllCloudConnectors(m_targetPeerAddress);
    for (auto& connector: m_connectors)
    {
        auto connectorType = connector.first;
        connector.second->connect(
            [connectorType, this](
                SystemError::ErrorCode errorCode,
                std::unique_ptr<AbstractTunnelConnection> connection)
            {
                onConnectorFinished(
                    connectorType,
                    errorCode,
                    std::move(connection));
            });
    }
}

void OutgoingTunnel::onConnectorFinished(
    CloudConnectType connectorType,
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractTunnelConnection> connection)
{
    QnMutexLocker lk(&m_mutex);

    const auto connectorIter = m_connectors.find(connectorType);
    assert(connectorIter != m_connectors.end());
    auto connector = std::move(connectorIter->second);
    m_connectors.erase(connectorIter);

    if (errorCode == SystemError::noError)
    {
        if (m_connection)
            return; //tunnel has already been connected, just ignoring this connection

        m_connection = std::move(connection);
        //we've connected to the host. Requesting client connections
        for (auto& connectRequest: m_connectHandlers)
        {
            using namespace std::placeholders;
            auto handler = std::move(connectRequest.second.handler);
            m_connection->establishNewConnection(
                std::move(connectRequest.second.timeout),   //TODO #ak recalculate timeout
                std::move(connectRequest.second.socketAttributes),
                [handler, this](    //TODO #ak #msvc2015 move to lambda
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> socket,
                    bool tunnelStillValid)
                {
                    onConnectFinished(
                        std::move(handler),
                        errorCode,
                        std::move(socket),
                        tunnelStillValid);
                });
        }

        m_connectHandlers.clear();
        return;
    }

    //connection failed
    if (!m_connectors.empty())
        return; //waiting for other connectors to complete

    //reporting error to everyone who is waiting
    auto connectHandlers = std::move(m_connectHandlers);
    lk.unlock();
    for (auto& connectRequest : connectHandlers)
        connectRequest.second.handler(errorCode, nullptr);
    //reporting tunnel failure
    onTunnelClosed();
}

std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>>
    OutgoingTunnel::createAllCloudConnectors(const AddressEntry& address)
{
    std::map<CloudConnectType, std::unique_ptr<AbstractTunnelConnector>> connectors;
    connectors.emplace(
        CloudConnectType::udtHp,
        std::make_unique<UdpHolePunchingTunnelConnector>(
            address));
    return connectors;
}

} // namespace cloud
} // namespace network
} // namespace nx
