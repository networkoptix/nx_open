#include "load_emulator.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/string.h>

#include <nx/vms/api/protocol_version.h>

namespace nx::cloud::db::test {

LoadEmulator::LoadEmulator(
    const std::string& cdbUrlStr,
    const std::string& accountEmail,
    const std::string& accountPassword)
    :
    m_cdbUrl(cdbUrlStr)
{
    m_cdbClient.setCloudUrl(m_cdbUrl);
    m_cdbClient.setCredentials(accountEmail, accountPassword);
    m_connectionHelper.setRemoveConnectionAfterClosure(true);

    nx::utils::Url cdbUrl(QString::fromStdString(m_cdbUrl));
    m_syncUrl = network::url::Builder().setScheme(cdbUrl.scheme())
        .setEndpoint(network::url::getEndpoint(cdbUrl));

    m_connectionHelper.onConnectionFailureSubscription().subscribe(
        [this](auto&&... args)
        {
            handleConnectionFailure(std::forward<decltype(args)>(args)...);
        },
        &m_connectionFailureSubscriptionId);
}

void LoadEmulator::setMaxDelayBeforeConnect(std::chrono::milliseconds delay)
{
    m_connectionHelper.setMaxDelayBeforeConnect(delay);
}

void LoadEmulator::setTransactionConnectionCount(int connectionCount)
{
    m_transactionConnectionCount = connectionCount;
}

void LoadEmulator::setReplaceFailedConnection(bool value)
{
    m_replaceFailedConnection = value;
}

void LoadEmulator::start()
{
    using namespace std::placeholders;

    // Fetching system list.
    m_cdbClient.systemManager()->getSystems(
        std::bind(&LoadEmulator::onSystemListReceived, this, _1, _2));
}

std::size_t LoadEmulator::activeConnectionCount() const
{
    return m_connectionHelper.activeConnectionCount();
}

std::size_t LoadEmulator::totalFailedConnections() const
{
    return m_connectionHelper.totalFailedConnections();
}

std::size_t LoadEmulator::connectedConnections() const
{
    return m_connectionHelper.connectedConnections();
}

void LoadEmulator::onSystemListReceived(
    api::ResultCode resultCode,
    api::SystemDataExList systems)
{
    NX_ASSERT(resultCode == api::ResultCode::ok);
    NX_ASSERT(!systems.systems.empty());
    m_systems = std::move(systems);

    openConnections();
}

void LoadEmulator::openConnections()
{
    std::size_t systemIndex = 0;
    for (int i = 0; i < m_transactionConnectionCount; ++i)
    {
        const auto& system = m_systems.systems[systemIndex];

        QnMutexLocker lock(&m_mutex);
        openConnection(lock, system);

        ++systemIndex;
        if (systemIndex == m_systems.systems.size())
            systemIndex = 0;
    }
}

void LoadEmulator::openConnection(
    const QnMutexLockerBase&,
    const api::SystemDataEx& system)
{
    const int connectionId = m_connectionHelper.establishTransactionConnection(
        m_syncUrl,
        system.id,
        system.authKey,
        KeepAlivePolicy::enableKeepAlive,
        nx::vms::api::protocolVersion());

    m_connections[connectionId] = system;
}

void LoadEmulator::handleConnectionFailure(
    int connectionId,
    ::ec2::QnTransactionTransportBase::State state)
{
    if (!m_replaceFailedConnection)
        return;

    QnMutexLocker lock(&m_mutex);

    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
        return;

    NX_INFO(this, "Reopening connection %1:%2", it->second.id, it->second.authKey);

    openConnection(lock, it->second);

    m_connections.erase(it);
}

} // namespace nx::cloud::db::test
