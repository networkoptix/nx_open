#include "load_emulator.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
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

    nx::utils::Url cdbUrl(QString::fromStdString(m_cdbUrl));
    m_syncUrl = network::url::Builder().setScheme(cdbUrl.scheme())
        .setEndpoint(network::url::getEndpoint(cdbUrl));
}

LoadEmulator::~LoadEmulator()
{
    for (const auto& test: m_tests)
        test->pleaseStopSync();
}

void LoadEmulator::setMaxDelayBeforeConnect(std::chrono::milliseconds delay)
{
    m_maxDelayBeforeConnect = delay;
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
    QnMutexLocker lock(&m_mutex);

    return std::accumulate(
        m_tests.begin(), m_tests.end(),
        (std::size_t) 0,
        [](std::size_t curSum, const auto& test) { return curSum + test->activeConnectionCount(); });
}

std::size_t LoadEmulator::totalFailedConnections() const
{
    QnMutexLocker lock(&m_mutex);

    return std::accumulate(
        m_tests.begin(), m_tests.end(),
        (std::size_t) 0,
        [](std::size_t curSum, const auto& test) { return curSum + test->totalFailedConnections(); });
}

std::size_t LoadEmulator::connectedConnections() const
{
    QnMutexLocker lock(&m_mutex);

    return std::accumulate(
        m_tests.begin(), m_tests.end(),
        (std::size_t) 0,
        [](std::size_t curSum, const auto& test) { return curSum + test->connectedConnections(); });
}

void LoadEmulator::onSystemListReceived(
    api::ResultCode resultCode,
    api::SystemDataExList systems)
{
    NX_ASSERT(resultCode == api::ResultCode::ok);
    NX_ASSERT(!systems.systems.empty());
    m_systems = std::move(systems);

    QnMutexLocker lock(&m_mutex);

    const auto aioThreads = nx::network::SocketGlobals::aioService().getAllAioThreads();

    int systemsPerTest = m_systems.systems.size() / aioThreads.size();
    if (systemsPerTest < 1)
        systemsPerTest = 1;
    auto curIt = m_systems.systems.begin();
    const auto endIt = m_systems.systems.end();

    auto remainder = m_transactionConnectionCount % (int)aioThreads.size();

    for (const auto& thread: aioThreads)
    {
        const auto rangeSize = std::min<int>(systemsPerTest, endIt - curIt);
        auto rangeEndIt = std::next(curIt, rangeSize);

        auto tester = std::make_unique<LoadTest>(
            m_syncUrl,
            std::vector<api::SystemDataEx>(curIt, rangeEndIt),
            m_maxDelayBeforeConnect,
            m_transactionConnectionCount / (int) aioThreads.size() + remainder,
            m_replaceFailedConnection);
        remainder = 0;
        tester->bindToAioThread(thread);
        tester->start();
        m_tests.push_back(std::move(tester));

        curIt += rangeSize;
    }
}

//-------------------------------------------------------------------------------------------------

LoadTest::LoadTest(
    nx::utils::Url syncUrl,
    std::vector<api::SystemDataEx> systems,
    std::chrono::milliseconds maxDelayBeforeConnect,
    int connectionCount,
    bool replaceFailedConnection)
    :
    m_syncUrl(syncUrl),
    m_systems(std::move(systems)),
    m_transactionConnectionCount(connectionCount),
    m_replaceFailedConnection(replaceFailedConnection)
{
    m_connectionHelper.setMaxDelayBeforeConnect(maxDelayBeforeConnect);
    m_connectionHelper.setRemoveConnectionAfterClosure(true);

    m_connectionHelper.onConnectionFailureSubscription().subscribe(
        [this](auto&&... args)
        {
            handleConnectionFailure(std::forward<decltype(args)>(args)...);
        },
        &m_connectionFailureSubscriptionId);
}

void LoadTest::start()
{
    openConnections();
}

void LoadTest::bindToAioThread(nx::network::aio::AbstractAioThread* thread)
{
    base_type::bindToAioThread(thread);
    m_connectionHelper.bindToAioThread(thread);
}

std::size_t LoadTest::activeConnectionCount() const
{
    return m_connectionHelper.activeConnectionCount();
}

std::size_t LoadTest::totalFailedConnections() const
{
    return m_connectionHelper.totalFailedConnections();
}

std::size_t LoadTest::connectedConnections() const
{
    return m_connectionHelper.connectedConnections();
}

void LoadTest::openConnections()
{
    std::size_t systemIndex = 0;
    for (int i = 0; i < m_transactionConnectionCount; ++i)
    {
        const auto& system = m_systems[systemIndex];

        QnMutexLocker lock(&m_mutex);
        openConnection(lock, system);

        ++systemIndex;
        if (systemIndex == m_systems.size())
            systemIndex = 0;
    }
}

void LoadTest::stopWhileInAioThread()
{
    m_connectionHelper.pleaseStopSync();
}

void LoadTest::openConnection(
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

void LoadTest::handleConnectionFailure(
    int connectionId,
    ::ec2::QnTransactionTransportBase::State state)
{
    if (!m_replaceFailedConnection)
        return;

    QnMutexLocker lock(&m_mutex);

    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
        return;

    NX_INFO(this, "Connection to %1:%2 ended up in state %3. Reopening",
        it->second.id, it->second.authKey, ::ec2::QnTransactionTransportBase::toString(state));

    openConnection(lock, it->second);

    m_connections.erase(it);
}

} // namespace nx::cloud::db::test
