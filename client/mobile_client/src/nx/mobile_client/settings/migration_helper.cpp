#include "migration_helper.h"

#include <functional>
#include <chrono>

#include <QtCore/QTimer>

#include <common/common_module.h>

#include <nx/utils/log/log.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/vms/discovery/manager.h>
#include <network/system_helpers.h>
#include <client_core/client_core_settings.h>
#include <mobile_client/mobile_client_settings.h>
#include <helpers/system_helpers.h>

#include "login_session.h"

namespace {

using namespace std::chrono;

static constexpr int kCheckIntervalMs = milliseconds(minutes(2)).count();

static QnUuid savedSessionId(const QVariant& sessionVariant)
{
    return QnLoginSession::fromVariant(sessionVariant.toMap()).id;
}

} // namespace

namespace nx {
namespace mobile_client {
namespace settings {

class SessionsMigrationHelper::Private: public QObject
{
public:
    Private(nx::vms::discovery::Manager* moduleDiscoveryManager);

    void checkEndpoints();
    void at_moduleFound(const vms::discovery::ModuleEndpoint& moduleData);

private:
    QTimer* m_checkTimer = nullptr;
    nx::vms::discovery::Manager* m_moduleDiscoveryManager = nullptr;
    QHash<QnUuid, nx::utils::Url> m_migratedSessions;
};

SessionsMigrationHelper::Private::Private(vms::discovery::Manager* moduleDiscoveryManager):
    m_moduleDiscoveryManager(moduleDiscoveryManager)
{
    if (!moduleDiscoveryManager)
    {
        NX_ASSERT(moduleDiscoveryManager);
        return;
    }

    for (const auto& sessionVariant: qnSettings->savedSessions())
    {
        const auto& session = QnLoginSession::fromVariant(sessionVariant.toMap());
        m_migratedSessions.insert(session.id, session.url);
    }

    checkEndpoints();

    if (!m_migratedSessions.isEmpty())
    {
        for (const auto& module: moduleDiscoveryManager->getAll())
            at_moduleFound(module);
    }

    if (m_migratedSessions.isEmpty())
        return;

    NX_DEBUG(this, "Starting check timer.");

    connect(moduleDiscoveryManager, &vms::discovery::Manager::found, this,
        &Private::at_moduleFound);

    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, this, &Private::checkEndpoints);
    m_checkTimer->start(kCheckIntervalMs);
}

void SessionsMigrationHelper::Private::checkEndpoints()
{
    for (auto it = m_migratedSessions.begin(); it != m_migratedSessions.end(); ++it)
    {
        NX_VERBOSE(this, lm("Checkig url %1").arg(it.value()));
        m_moduleDiscoveryManager->checkEndpoint(it.value());
    }
}

void SessionsMigrationHelper::Private::at_moduleFound(
    const vms::discovery::ModuleEndpoint& moduleData)
{
    using client::core::LocalConnectionData;

    const auto systemId = helpers::getLocalSystemId(moduleData);
    auto recentConnections = qnClientCoreSettings->recentLocalConnections();
    auto authenticationData = qnClientCoreSettings->systemAuthenticationData();
    auto savedSessions = qnSettings->savedSessions();
    bool recentConnectionsChanged = false;

    auto findConnection =
        [&recentConnections](
            std::function<bool(const QnUuid&, const LocalConnectionData&)> check)
        {
            auto it = recentConnections.begin();
            for (; it != recentConnections.end(); ++it)
            {
                if (check(it.key(), it.value()))
                    break;
            }
            return it;
        };

    while (!m_migratedSessions.empty())
    {
        // There could be multiple sessions with the same URL and even same credentials.

        auto migratedIt = findConnection(
            [this, &moduleData](
                const QnUuid& localId, const LocalConnectionData& data)
            {
                if (!m_migratedSessions.contains(localId))
                    return false;

                for (const auto& url: data.urls)
                {
                    if (moduleData.endpoint == nx::network::url::getEndpoint(url))
                        return true;
                }

                return false;
            });

        if (migratedIt == recentConnections.end())
            break;

        const auto fakeId = migratedIt.key();
        const auto connectionData = migratedIt.value();
        const auto credentialsList = authenticationData.take(fakeId);

        recentConnections.erase(migratedIt);
        client::core::helpers::removeConnection(fakeId);

        savedSessions.erase(
            std::find_if(savedSessions.begin(), savedSessions.end(),
                [&fakeId](const QVariant& sessionVariant)
                {
                    return fakeId == savedSessionId(sessionVariant);
                }));

        for (const auto& url: connectionData.urls)
            client::core::helpers::storeConnection(systemId, connectionData.systemName, url);

        for (const auto& credentials: credentialsList)
        {
            if (client::core::helpers::getCredentials(systemId, credentials.user).isEmpty())
                client::core::helpers::storeCredentials(systemId, credentials);
        }

        recentConnectionsChanged = true;
    }

    if (recentConnectionsChanged)
    {
        qnSettings->setSavedSessions(savedSessions);

        if (savedSessions.isEmpty())
        {
            m_checkTimer->stop();
            NX_DEBUG(this, "No saved connections left. Stopping.");
        }
    }
}

SessionsMigrationHelper::SessionsMigrationHelper(QObject* parent):
    QObject(parent),
    d(new Private(commonModule()->moduleDiscoveryManager()))
{
}

SessionsMigrationHelper::~SessionsMigrationHelper()
{
}

} // namespace settings
} // namespace mobile_client
} // namespace nx
