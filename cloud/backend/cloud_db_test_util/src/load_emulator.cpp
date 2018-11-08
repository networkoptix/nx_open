#include "load_emulator.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/string.h>

namespace nx {
namespace cdb {
namespace test {

LoadEmulator::LoadEmulator(
    const std::string& cdbUrl,
    const std::string& accountEmail,
    const std::string& accountPassword)
    :
    m_cdbUrl(cdbUrl),
    m_transactionConnectionCount(100)
{
    m_cdbClient.setCloudUrl(m_cdbUrl);
    m_cdbClient.setCredentials(accountEmail, accountPassword);
    m_connectionHelper.setRemoveConnectionAfterClosure(true);
}

void LoadEmulator::setMaxDelayBeforeConnect(std::chrono::milliseconds delay)
{
    m_connectionHelper.setMaxDelayBeforeConnect(delay);
}

void LoadEmulator::setTransactionConnectionCount(int connectionCount)
{
    m_transactionConnectionCount = connectionCount;
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
    QUrl cdbUrl(QString::fromStdString(m_cdbUrl));
    const network::SocketAddress cdbEndpoint = network::url::getEndpoint(cdbUrl);

    std::size_t systemIndex = 0;
    for (int i = 0; i < m_transactionConnectionCount; ++i)
    {
        const auto& system = m_systems.systems[systemIndex];

        m_connectionHelper.establishTransactionConnection(
            network::url::Builder().setScheme(cdbUrl.scheme())
                .setHost(cdbEndpoint.address.toString())
                .setPort(cdbEndpoint.port),
            system.id,
            system.authKey,
            KeepAlivePolicy::enableKeepAlive,
            nx_ec::EC2_PROTO_VERSION);

        ++systemIndex;
        if (systemIndex == m_systems.systems.size())
            systemIndex = 0;
    }
}

} // namespace test
} // namespace cdb
} // namespace nx
