#include "module_connector.h"

#include <nx/utils/log/log.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace discovery {

static const auto kUrl = lit("http://%1/api/moduleInformation?showAddresses=false");
static const KeepAliveOptions kKeepAliveOptions(60, 10, 3);

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

void ModuleConnector::newEndpoint(const SocketAddress& endpoint, const QnUuid& id)
{
    dispatch(
        [this, endpoint, id]()
        {
            getModule(id)->addEndpoint(endpoint);
        });
}

void ModuleConnector::ignoreEndpoints(std::set<SocketAddress> endpoints, const QnUuid& id)
{
    dispatch(
        [this, endpoints = std::move(endpoints), id]()
        {
            getModule(id)->forbidEndpoints(std::move(endpoints));
        });
}

void ModuleConnector::activate()
{
    dispatch(
        [this]()
        {
            m_isPassiveMode = false;
            for (const auto& module: m_modules)
                module.second->ensureConnect();
        });
}

void ModuleConnector::diactivate()
{
    dispatch(
        [this]()
        {
            m_isPassiveMode = true;
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
    NX_LOGX(lm("New %1").strs(m_id), cl_logDEBUG1);
}

ModuleConnector::Module::~Module()
{
    NX_LOGX(lm("Delete %1").strs(m_id), cl_logDEBUG1);
    NX_ASSERT(m_timer.isInSelfAioThread());
    m_socket.reset();
    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
}

void ModuleConnector::Module::addEndpoint(const SocketAddress& endpoint)
{
    if (m_id.isNull())
        m_endpoints[kDefault].insert(endpoint); // Do not sort endpoints for unknown server.
    else if (endpoint.address == HostAddress::localhost)
        m_endpoints[kLocalHost].insert(endpoint);
    else if (endpoint.address.isLocal())
        m_endpoints[kLocalNetwork].insert(endpoint);
    else if (endpoint.address.isIpAddress())
        m_endpoints[kIp].insert(endpoint); // TODO: check if we have such interface?
    else
        m_endpoints[kOther].insert(endpoint);

    ensureConnect();
}

void ModuleConnector::Module::ensureConnect()
{
    if ((m_id.isNull()) || (m_httpClients.empty() && !m_socket))
        connect(m_endpoints.begin());
}

void ModuleConnector::Module::forbidEndpoints(std::set<SocketAddress> endpoints)
{
    NX_ASSERT(!m_id.isNull(), "Does it make sense to block endpoints for unknown servers?");
    m_forbiddenEndpoints = std::move(endpoints);
}

void ModuleConnector::Module::connect(Endpoints::iterator endpointsGroup)
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
            NX_LOGX(lm("No more endpoints, retry in %1").str(reconnectInterval), cl_logDEBUG2);
            m_timer.start(reconnectInterval, [this](){ connect(m_endpoints.begin()); });
            m_parent->m_disconnectedHandler(m_id);
        }

        return;
    }

    size_t endpointsInProgress = 0;
    m_timer.cancelSync();
    for (const auto& endpoint: endpointsGroup->second)
    {
        if (m_forbiddenEndpoints.count(endpoint))
            continue;

        ++endpointsInProgress;
        connect(endpoint, endpointsGroup);
    }

    if (endpointsInProgress == 0)
        return connect(++endpointsGroup);
}

void ModuleConnector::Module::connect(
    const SocketAddress& endpoint, Endpoints::iterator endpointsGroup)
{
    NX_LOGX(lm("Attempt to connect to %1 by %2").strs(m_id, endpoint), cl_logDEBUG2);
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

            if (!m_httpClients.empty())
                return; //< Wait for other HTTP clients.

            connect(++endpointsGroup);
        });
}

boost::optional<QnModuleInformation> ModuleConnector::Module::getInformation(
    nx_http::AsyncHttpClientPtr client)
{
    if (!client->hasRequestSuccesed())
    {
        NX_LOGX(lm("Request to %1 has failed").strs(client->url()), cl_logDEBUG1);
        return boost::none;
    }

    const auto contentType = nx_http::getHeaderValue(
        client->response()->headers, "Content-Type");

    if (Qn::serializationFormatFromHttpContentType(contentType) != Qn::JsonFormat)
    {
        NX_LOGX(lm("Unexpected Content-Type %2 from %3")
            .strs(contentType, client->url()), cl_logDEBUG2);
        return boost::none;
    }

    QnJsonRestResult restResult;
    if (!QJson::deserialize(client->fetchMessageBodyBuffer(), &restResult)
        || restResult.error != QnRestResult::Error::NoError)
    {
        NX_LOGX(lm("Error response '%2' from %3")
            .strs(restResult.errorString, client->url()), cl_logDEBUG2);
        return boost::none;
    }

    QnModuleInformation moduleInformation;
    if (!QJson::deserialize<QnModuleInformation>(restResult.reply, &moduleInformation)
        || moduleInformation.id.isNull())
    {
        NX_LOGX(lm("Can not desserialize rsponse from %1")
            .strs(moduleInformation.id), cl_logDEBUG2);
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

    NX_LOGX(lm("Connected to %1").strs(m_id), cl_logDEBUG2);
    m_parent->m_connectedHandler(information, std::move(endpoint));

    // TODO: Currently mediaserver keepAlive timeout is set to 5 seconds, it means we will go
    // for reconnect attempt every timeout. It looks like we do not have any options for
    // old servers.
    // For new servers we could use WebSocket streams to reduce traffic to WebSocket keep alives.
    auto socket = client->takeSocket();
    if (!socket->setRecvTimeout(0) || !socket->setKeepAlive(kKeepAliveOptions))
    {
        NX_LOGX(lm("Unable to save connection: %1").strs(
            SystemError::getLastOSErrorText()), cl_logWARNING);

        return false;
    }

    const auto buffer = std::make_shared<Buffer>();
    buffer->reserve(100);

    m_socket = std::move(socket);
    m_socket->readSomeAsync(buffer.get(),
        [this, buffer](SystemError::ErrorCode code, size_t size)
        {
            NX_LOGX(lm("Unexpectd connection read size=%1: %2").strs(
                size, SystemError::toString(code)), cl_logDEBUG2);

            m_socket.reset();
            connect(m_endpoints.begin()); //< Reconnect attempt.
        });

    return true;
}

ModuleConnector::Module* ModuleConnector::getModule(const QnUuid& id)
{
    const auto it = m_modules.emplace(id, std::unique_ptr<Module>());
    if (it.second)
        it.first->second = std::make_unique<Module>(this, id);

    return it.first->second.get();
}

} // namespace discovery
} // namespace vms
} // namespace nx
