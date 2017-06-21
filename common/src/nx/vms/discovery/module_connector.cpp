#include "module_connector.h"

#include <nx/utils/log/log.h>
#include <rest/server/json_rest_result.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace vms {
namespace discovery {

static const auto kUrl = lit("http://%1/api/moduleInformation?showAddresses=false&keepConnectionOpen");
static const KeepAliveOptions kKeepAliveOptions(
    std::chrono::seconds(60), std::chrono::seconds(10), 3);

ModuleConnector::ModuleConnector(network::aio::AbstractAioThread* thread):
    network::aio::BasicPollable(thread),
    m_reconnectInterval(std::chrono::minutes(1))
{
}

void ModuleConnector::setReconnectInterval(std::chrono::milliseconds interval)
{
    m_reconnectInterval = interval;
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
    m_id(id)
{
    m_timer.bindToAioThread(parent->getAioThread());
    NX_DEBUG(this, lm("New %1").args(m_id));
}

ModuleConnector::Module::~Module()
{
    NX_DEBUG(this, lm("Delete %1").args(m_id));
    NX_ASSERT(m_timer.isInSelfAioThread());
    m_socket.reset();
    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
}

void ModuleConnector::Module::addEndpoints(std::set<SocketAddress> endpoints)
{
    NX_VERBOSE(this, lm("Add endpoints %1").container(endpoints));
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

    if (m_id.isNull())
    {
        // For unknown server connect to every new endpoint.
        const auto group = getGroup(kDefault);
        for (const auto& endpoint: endpoints)
        {
            if (insertIntoGroup(group, endpoint) && !m_parent->m_isPassiveMode)
                connectToEndpoint(endpoint, group);
        }
    }
    else
    {
        // For known server sort endpoints by accessibility type and connect by order.
        bool hasNewEndpoints = false;
        for (auto& endpoint: endpoints)
        {
            Endpoints::iterator group;
            if (endpoint.address == HostAddress::localhost)
                group = getGroup(kLocalHost);
            else if (endpoint.address.isLocal())
                group = getGroup(kLocalNetwork);
            else if (endpoint.address.isIpAddress())
                group = getGroup(kIp); //< TODO: Consider to check if we have such interface.
            else
                group = getGroup(kOther);

            hasNewEndpoints |= insertIntoGroup(group, std::move(endpoint));
        }

        if (hasNewEndpoints)
            ensureConnection();
    }
}

void ModuleConnector::Module::ensureConnection()
{
    if ((m_id.isNull()) || (m_httpClients.empty() && !m_socket))
        connectToGroup(m_endpoints.begin());
}

void ModuleConnector::Module::setForbiddenEndpoints(std::set<SocketAddress> endpoints)
{
    NX_VERBOSE(this, lm("Forbid endpoints %1").container(endpoints));
    NX_ASSERT(!m_id.isNull(), "Does not make sense to block endpoints for unknown servers");
    m_forbiddenEndpoints = std::move(endpoints);
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
            const auto reconnectInterval = m_parent->m_reconnectInterval;
            NX_VERBOSE(this, lm("No more endpoints, retry in %1").arg(reconnectInterval));
            m_timer.start(reconnectInterval, [this](){ connectToGroup(m_endpoints.begin()); });
            m_parent->m_disconnectedHandler(m_id);
        }

        return;
    }

    // Initiate parallel connects to each endpoint in a group.
    size_t endpointsInProgress = 0;
    m_timer.cancelSync();
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
    client->bindToAioThread(m_timer.getAioThread());
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
    if (m_socket || m_id.isNull())
        return true;

    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
    m_httpClients.clear();

    // TODO: Currently mediaserver keepAlive timeout is set to 5 seconds, it means we will go
    // for reconnect attempt every timeout. It looks like we do not have any options for
    // old servers.
    // For new servers we could use WebSocket streams to reduce traffic to WebSocket keep alives.
    auto socket = client->takeSocket();
    if (!socket->setRecvTimeout(0) || !socket->setKeepAlive(kKeepAliveOptions))
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
            NX_VERBOSE(this, lm("Unexpectd connection read size=%1: %2").args(
                size, SystemError::toString(code)));

            m_socket.reset();
            connectToGroup(m_endpoints.begin()); //< Reconnect attempt.
        });

    auto ip = m_socket->getForeignAddress().address;
    NX_VERBOSE(this, lm("Connected to %1 by %2 ip %3").args(m_id, endpoint, ip));
    m_parent->m_connectedHandler(information, std::move(endpoint), std::move(ip));
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
