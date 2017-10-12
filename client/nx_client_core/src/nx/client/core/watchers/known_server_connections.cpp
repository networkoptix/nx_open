#include "known_server_connections.h"

#include <chrono>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/vms/discovery/manager.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <client/client_message_processor.h>
#include <client_core/client_core_settings.h>

namespace {

using namespace std::chrono;

constexpr milliseconds kCheckInterval = minutes(2);

template<typename T>
bool trimConnectionsList(QList<T>& list)
{
    constexpr int kMaxItemsToStore = 8;

    if (list.size() <= kMaxItemsToStore)
        return false;

    list.erase(list.begin() + kMaxItemsToStore, list.end());
    return true;
}

} // namespace

namespace nx {
namespace client {
namespace core {
namespace watchers {

class KnownServerConnections::Private: public QObject, public QnCommonModuleAware
{
public:
    Private(QnCommonModule* commonModule);

    void start();

private:
    void checkKnownUrls();
    void saveConnection(Connection connection);

    void at_moduleFound(const vms::discovery::ModuleEndpoint& moduleData);

private:
    nx::vms::discovery::Manager* m_discoveryManager = nullptr;
    QList<KnownServerConnections::Connection> m_connections;
    QPointer<QTimer> m_timer;
};

KnownServerConnections::Private::Private(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_discoveryManager(commonModule->moduleDiscoveryManager())
{
    if (!m_discoveryManager)
        NX_ASSERT(m_discoveryManager);
}

void KnownServerConnections::Private::start()
{
    m_connections = qnClientCoreSettings->knownServerConnections();
    if (trimConnectionsList(m_connections))
        qnClientCoreSettings->setKnownServerConnections(m_connections);

    for (const auto& connection: m_connections)
        m_discoveryManager->checkEndpoint(connection.url, connection.serverId);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        [this]()
        {
            saveConnection(Connection{commonModule()->remoteGUID(), commonModule()->currentUrl()});
        });

    if (!qnClientCoreSettings->knownServerUrls().isEmpty())
    {
        m_timer = new QTimer(this);
        connect(m_timer.data(), &QTimer::timeout, this, &Private::checkKnownUrls);

        connect(m_discoveryManager, &vms::discovery::Manager::found, this,
            &Private::at_moduleFound);

        m_timer->start(kCheckInterval.count());

        checkKnownUrls();
    }
}

void KnownServerConnections::Private::checkKnownUrls()
{
    const auto& urls = qnClientCoreSettings->knownServerUrls();
    if (urls.isEmpty())
    {
        NX_VERBOSE(this, "No more known URLs left. Stopping periodical checks.");

        m_discoveryManager->disconnect(this);
        delete m_timer;
        return;
    }

    NX_VERBOSE(this, "Periodical check for known urls.");

    for (const auto& url: urls)
    {
        NX_VERBOSE(this, lm("Checking URL: %1").arg(url));
        m_discoveryManager->checkEndpoint(url);
    }
}

void KnownServerConnections::Private::saveConnection(Connection connection)
{
    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(connection.url.host()))
        return;

    connection.url = nx::network::url::Builder()
        .setScheme(lit("http"))
        .setHost(connection.url.host())
        .setPort(connection.url.port());

    NX_VERBOSE(this,
        lm("Saving connection, id=%1 url=%2").arg(connection.serverId).arg(connection.url));

    // Place url to the top of the list.
    m_connections.removeOne(connection);
    m_connections.prepend(connection);
    trimConnectionsList(m_connections);

    m_discoveryManager->checkEndpoint(connection.url, connection.serverId);
    qnClientCoreSettings->setKnownServerConnections(m_connections);
}

void KnownServerConnections::Private::at_moduleFound(
    const vms::discovery::ModuleEndpoint& moduleData)
{
    auto urls = qnClientCoreSettings->knownServerUrls();

    auto it = std::find_if(urls.begin(), urls.end(),
        [&moduleData](const QUrl& url)
        {
            return moduleData.endpoint == nx::network::url::getEndpoint(url);
        });

    if (it == urls.end())
        return;

    NX_VERBOSE(this, lm("Converting known URL to known connection: %1").arg(*it));
    urls.erase(it);

    saveConnection(Connection{
        moduleData.id,
        nx::network::url::Builder()
            .setScheme(lit("http"))
            .setEndpoint(moduleData.endpoint)});

    qnClientCoreSettings->setKnownServerUrls(urls);
}

KnownServerConnections::KnownServerConnections(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    d(new Private(commonModule))
{
}

KnownServerConnections::~KnownServerConnections()
{
}

void KnownServerConnections::start()
{
    d->start();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(KnownServerConnections::Connection, (json)(eq), (serverId)(url))

} // namespace watchers
} // namespace core
} // namespace client
} // namespace nx
