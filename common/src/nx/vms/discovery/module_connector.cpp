#include "module_connector.h"

#include <nx/utils/log/log.h>
#include <rest/server/json_rest_result.h>
#include <nx/network/socket_global.h>
#include <nx/utils/app_info.h>

namespace nx {
namespace vms {
namespace discovery {

static const auto kUrl =
    lit("http://%1/api/moduleInformation?showAddresses=false&keepConnectionOpen");

std::chrono::seconds kDefaultDisconnectTimeout(10);
static const network::RetryPolicy kDefaultRetryPolicy(
    network::RetryPolicy::kInfiniteRetries, std::chrono::seconds(5), 2, std::chrono::minutes(1));

ModuleConnector::ModuleConnector(network::aio::AbstractAioThread* thread):
    network::aio::BasicPollable(thread),
    m_disconnectTimeout(kDefaultDisconnectTimeout),
    m_retryPolicy(kDefaultRetryPolicy)
{
}

void ModuleConnector::setDisconnectTimeout(std::chrono::milliseconds value)
{
    NX_ASSERT(m_modules.size() == 0);
    m_disconnectTimeout = value;
}

void ModuleConnector::setReconnectPolicy(network::RetryPolicy value)
{
    NX_ASSERT(m_modules.size() == 0);
    m_retryPolicy = value;
}

void ModuleConnector::setConnectHandler(ConnectedHandler handler)
{
    m_connectedHandler = std::move(handler);
}

void ModuleConnector::setDisconnectHandler(DisconnectedHandler handler)
{
    m_disconnectedHandler = std::move(handler);
}

static void validateEndpoints(std::set<SocketAddress>* endpoints)
{
    const auto& resolver = nx::network::SocketGlobals::addressResolver();
    for (auto it = endpoints->begin(); it != endpoints->end(); )
    {
        NX_ASSERT(resolver.isValidForConnect(*it), lm("Invalid endpoint: %1").arg(*it));

        if (resolver.isValidForConnect(*it))
            ++it;
        else
            it = endpoints->erase(it);
    }
}

void ModuleConnector::newEndpoints(std::set<SocketAddress> endpoints, const QnUuid& id)
{
    validateEndpoints(&endpoints);
    NX_ASSERT(endpoints.size());
    if (endpoints.empty())
        return;

    dispatch(
        [this, endpoints = std::move(endpoints), id]() mutable
        {
            getModule(id)->addEndpoints(std::move(endpoints));
        });
}

void ModuleConnector::setForbiddenEndpoints(std::set<SocketAddress> endpoints, const QnUuid& id)
{
    validateEndpoints(&endpoints);
    dispatch(
        [this, endpoints = std::move(endpoints), id]() mutable
        {
            getModule(id)->setForbiddenEndpoints(std::move(endpoints));
        });
}

void ModuleConnector::activate()
{
    dispatch(
        [this]()
        {
            m_isPassiveMode = false;
            NX_DEBUG(this, "Activated");

            for (const auto& module: m_modules)
                module.second->ensureConnection();
        });
}

void ModuleConnector::deactivate()
{
    dispatch(
        [this]()
        {
            m_isPassiveMode = true;
            NX_DEBUG(this, "Diactivated");
        });
}

void ModuleConnector::stopWhileInAioThread()
{
    m_modules.clear();
}

ModuleConnector::Module::Module(ModuleConnector* parent, const QnUuid& id):
    m_parent(parent),
    m_id(id),
    m_reconnectTimer(parent->m_retryPolicy, parent->getAioThread()),
    m_disconnectTimer(parent->getAioThread())
{
    NX_DEBUG(this, lm("New %1").args(m_id));
}

ModuleConnector::Module::~Module()
{
    NX_DEBUG(this, lm("Delete %1").args(m_id));
    NX_ASSERT(m_reconnectTimer.isInSelfAioThread());
    m_socket.reset();
    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
}

void ModuleConnector::Module::addEndpoints(std::set<SocketAddress> endpoints)
{
    NX_VERBOSE(this, lm("Add endpoints %1").container(endpoints));
    if (m_id.isNull())
    {
        // For unknown server connect to every new endpoint.
        for (const auto& endpoint: endpoints)
        {
            if (auto group = saveEndpoint(endpoint))
            {
                if (!m_parent->m_isPassiveMode)
                    connectToEndpoint(endpoint, group.get());
            }
        }
    }
    else
    {
        // For known server sort endpoints by accessibility type and connect by order.
        bool hasNewEndpoints = false;
        for (auto& endpoint: endpoints)
            hasNewEndpoints |= (bool) saveEndpoint(std::move(endpoint));

        if (hasNewEndpoints)
            ensureConnection();
    }
}

void ModuleConnector::Module::ensureConnection()
{
    if (m_id.isNull() || (m_httpClients.empty() && !m_socket))
        connectToGroup(m_endpoints.begin());
}

void ModuleConnector::Module::setForbiddenEndpoints(std::set<SocketAddress> endpoints)
{
    NX_VERBOSE(this, lm("Forbid endpoints %1").container(endpoints));
    NX_ASSERT(!m_id.isNull(), "Does not make sense to block endpoints for unknown servers");
    m_forbiddenEndpoints = std::move(endpoints);
}

ModuleConnector::Module::Priority
    ModuleConnector::Module::hostPriority(const HostAddress& host) const
{
    if (m_id.isNull())
        return kDefault;

    if (host == HostAddress::localhost)
        return kLocalHost;

    if (host.isLocal())
        return kLocalNetwork;

    if (host.isIpAddress())
        return kIp; //< TODO: Consider to check if we have such interface.

    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(host.toString()))
        return kCloud;

    return kOther;
}

boost::optional<ModuleConnector::Module::Endpoints::iterator>
    ModuleConnector::Module::saveEndpoint(SocketAddress endpoint)
{
    const auto getGroup =
        [&](Priority p)
        {
            return m_endpoints.emplace(p, std::set<SocketAddress>()).first;
        };

    const auto insertIntoGroup =
        [&](Endpoints::iterator groupIterator, SocketAddress endpoint)
        {
            auto& group = groupIterator->second;
            return group.insert(std::move(endpoint)).second;
        };

    const auto group = getGroup(hostPriority(endpoint.address));
    if (insertIntoGroup(group, endpoint))
    {
        NX_DEBUG(this, lm("Save endpoint %1 to %2").args(endpoint, group->first));
        return group;
    }

    return boost::none;
}

void ModuleConnector::Module::connectToGroup(Endpoints::iterator endpointsGroup)
{
    if (m_parent->m_isPassiveMode)
    {
        if (!m_id.isNull())
            m_parent->m_disconnectedHandler(m_id);

        return;
    }

    if (endpointsGroup == m_endpoints.end())
    {
        if (!m_id.isNull())
        {
            m_reconnectTimer.scheduleNextTry([this](){ connectToGroup(m_endpoints.begin()); });
            NX_VERBOSE(this, lm("No more endpoints, retry in %1").arg(m_reconnectTimer.currentDelay()));
            m_parent->m_disconnectedHandler(m_id);
        }

        return;
    }

    if (m_socket)
        return;

    NX_VERBOSE(this, lm("Connect to group %1: %2").args(
        endpointsGroup->first, containerString(endpointsGroup->second)));

    if (m_reconnectTimer.timeToEvent())
        m_reconnectTimer.cancelSync();

    // Initiate parallel connects to each endpoint in a group.
    size_t endpointsInProgress = 0;
    for (const auto& endpoint: endpointsGroup->second)
    {
        if (m_forbiddenEndpoints.count(endpoint))
            continue;

        ++endpointsInProgress;
        connectToEndpoint(endpoint, endpointsGroup);
    }

    // Go to a next group if we could not start any connects in current group.
    if (endpointsInProgress == 0)
        return connectToGroup(++endpointsGroup);
}

void ModuleConnector::Module::connectToEndpoint(
    const SocketAddress& endpoint, Endpoints::iterator endpointsGroup)
{
    NX_VERBOSE(this, lm("Attempt to connect to %1 by %2").args(m_id, endpoint));
    const auto client = nx_http::AsyncHttpClient::create();
    m_httpClients.insert(client);
    client->bindToAioThread(m_reconnectTimer.getAioThread());
    client->doGet(kUrl.arg(endpoint.toString()),
        [this, endpoint, endpointsGroup](nx_http::AsyncHttpClientPtr client) mutable
        {
            m_httpClients.erase(client);
            const auto moduleInformation = getInformation(client);
            if (moduleInformation)
            {
                if (moduleInformation->id == m_id)
                {
                    if (saveConnection(endpoint, std::move(client), *moduleInformation))
                        return;
                }
                else
                {
                    // Transfer this connection to a different module.
                    endpointsGroup->second.erase(endpoint);
                    m_parent->getModule(moduleInformation->id)
                        ->saveConnection(endpoint, std::move(client), *moduleInformation);
                }
            }

            if (m_id.isNull())
            {
                endpointsGroup->second.erase(endpoint);
                return;
            }

            // When the last endpoint in a group fails try the next group.
            if (m_httpClients.empty())
                connectToGroup(++endpointsGroup);
        });
}

boost::optional<QnModuleInformation> ModuleConnector::Module::getInformation(
    nx_http::AsyncHttpClientPtr client)
{
    if (!client->hasRequestSuccesed())
    {
        NX_DEBUG(this, lm("Request to %1 has failed").args(client->url()));
        return boost::none;
    }

    const auto contentType = nx_http::getHeaderValue(
        client->response()->headers, "Content-Type");

    if (Qn::serializationFormatFromHttpContentType(contentType) != Qn::JsonFormat)
    {
        NX_DEBUG(this, lm("Unexpected Content-Type %2 from %3")
            .args(contentType, client->url()));
        return boost::none;
    }

    QnJsonRestResult restResult;
    if (!QJson::deserialize(client->fetchMessageBodyBuffer(), &restResult)
        || restResult.error != QnRestResult::Error::NoError)
    {
        NX_DEBUG(this, lm("Error response '%2' from %3")
            .args(restResult.errorString, client->url()));
        return boost::none;
    }

    QnModuleInformation moduleInformation;
    if (!QJson::deserialize<QnModuleInformation>(restResult.reply, &moduleInformation)
        || moduleInformation.id.isNull())
    {
        NX_DEBUG(this, lm("Can not deserialize rsponse from %1")
            .args(moduleInformation.id));
        return boost::none;
    }

    return moduleInformation;
}

bool ModuleConnector::Module::saveConnection(
   SocketAddress endpoint, nx_http::AsyncHttpClientPtr client, const QnModuleInformation& information)
{
    NX_ASSERT(!m_id.isNull());
    if (m_id.isNull())
        return false;

    saveEndpoint(endpoint);
    if (m_socket)
        return true;

    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
    m_httpClients.clear();

    auto socket = client->takeSocket();
    if (!socket->setRecvTimeout(m_parent->m_disconnectTimeout))
    {
        NX_WARNING(this, lm("Unable to save connection to %1: %2").args(
            m_id, SystemError::getLastOSErrorText()));

        return false;
    }

    const auto buffer = std::make_shared<Buffer>();
    buffer->reserve(1);

    m_socket = std::move(socket);
    m_socket->readSomeAsync(buffer.get(),
        [this, buffer](SystemError::ErrorCode code, size_t size)
        {
            NX_VERBOSE(this, lm("Unexpected connection to %1 read size=%2: %3").args(
                m_id, size, SystemError::toString(code)));

            m_socket.reset();
            ensureConnection(); //< Reconnect attempt.

            // Make sure we report disconnect in a limited time.
            m_disconnectTimer.start(m_parent->m_disconnectTimeout * m_endpoints.size(),
                [this](){ m_parent->m_disconnectedHandler(m_id); });
        });

    auto ip = m_socket->getForeignAddress().address;
    NX_VERBOSE(this, lm("Connected to %1 by %2 ip %3").args(m_id, endpoint, ip));
    m_parent->m_connectedHandler(information, std::move(endpoint), std::move(ip));
    m_reconnectTimer.reset();
    m_disconnectTimer.cancelSync();
    return true;
}

ModuleConnector::Module* ModuleConnector::getModule(const QnUuid& id)
{
    auto& module = m_modules[id];
    if (!module)
        module = std::make_unique<Module>(this, id);

    return module.get();
}

} // namespace discovery
} // namespace vms
} // namespace nx
