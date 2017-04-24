#include "module_connector.h"

#include <nx/utils/log/log.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace discovery {

static const std::chrono::milliseconds kReconnectInterval = std::chrono::minutes(1);
static const auto kUrl = lit("http://%1/api/moduleInformation?showAddresses=false");
static const KeepAliveOptions kKeepAliveOptions(60, 10, 3);

ModuleConnector::ModuleConnector(network::aio::AbstractAioThread* thread):
    network::aio::BasicPollable(thread)
{
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
    getModule(id)->addEndpoint(endpoint);
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
    NX_LOGX(lm("New %1").strs(id), cl_logDEBUG1);
}

ModuleConnector::Module::~Module()
{
    NX_ASSERT(m_timer.isInSelfAioThread());
    m_socket.reset();
    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
}

void ModuleConnector::Module::addEndpoint(const SocketAddress& endpoint)
{
    if (endpoint.address.isLocal())
        m_endpoints[kLocal].insert(endpoint);
    else if (endpoint.address.isIpAddress())
        m_endpoints[kIp].insert(endpoint); // TODO: check if we have such interface?
    else
        m_endpoints[kOther].insert(endpoint);

    if (m_httpClients.empty() && !m_socket)
        connect(m_endpoints.begin());
}

void ModuleConnector::Module::connect(Endpoints::iterator endpointsGroup)
{
    if (endpointsGroup == m_endpoints.end())
    {
        NX_LOGX(lm("No more endpoints, retry in %1").strs(kReconnectInterval), cl_logDEBUG2);
        m_parent->m_disconnectedHandler(m_id);
        m_timer.start(kReconnectInterval, [this](){ connect(m_endpoints.begin()); });
        return;
    }

    if (endpointsGroup->second.empty())
        return connect(++endpointsGroup);

    m_timer.cancelSync();
    for (const auto& endpoint: endpointsGroup->second)
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
                        return saveConnection(endpoint, std::move(client), *moduleInformation);

                    // Transfer this connection to a different module.
                    endpointsGroup->second.erase(endpoint);
                    m_parent->getModule(moduleInformation->id)
                        ->saveConnection(endpoint, std::move(client), *moduleInformation);
                }

                if (!m_httpClients.empty())
                    return; //< Wait for other HTTP clients.

                connect(++endpointsGroup);
            });
    }
}

boost::optional<QnModuleInformation> ModuleConnector::Module::getInformation(
    nx_http::AsyncHttpClientPtr client)
{
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

void ModuleConnector::Module::saveConnection(
   SocketAddress endpoint, nx_http::AsyncHttpClientPtr client, const QnModuleInformation& information)
{
    if (m_socket)
        return;

    for (const auto& client: m_httpClients)
        client->pleaseStopSync();
    m_httpClients.clear();

    NX_LOGX(lm("Connected to %1").strs(m_id), cl_logDEBUG1);
    m_parent->m_connectedHandler(information, std::move(endpoint));

    // TODO: It is better to reask for moduleInformation instead of connection failure wait,
    // but is easier for now.
    const auto buffer = std::make_shared<Buffer>();
    buffer->reserve(100);
    m_socket = client->takeSocket();
    m_socket->setKeepAlive(kKeepAliveOptions);
    m_socket->readSomeAsync(buffer.get(),
        [this, buffer](SystemError::ErrorCode code, size_t size)
        {
            NX_LOGX(lm("Unexpectd connection read size=%1: %2").strs(
                size, SystemError::toString(code)), cl_logDEBUG1);

            m_socket.reset();
            connect(m_endpoints.begin()); //< Reconnect attempt.
        });
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
