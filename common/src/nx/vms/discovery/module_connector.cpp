#include "module_connector.h"

#include <nx/utils/log/log.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace discovery {

static const std::chrono::milliseconds kReconnectInterval = std::chrono::minutes(1);
static const auto kUrl = lit("http://%1/api/moduleInformation?showAddresses=false");
static const KeepAliveOptions kKeepAliveOptions(60, 10, 3);

ModuleConnector::ModuleConnector(
    ConnectedHandler connectedHandler,
    DisconnectedHandler disconnectedHandler)
:
    m_connectedHandler(std::move(connectedHandler)),
    m_disconnectedHandler(std::move(disconnectedHandler))
{
}

void ModuleConnector::newEndpoint(const QnUuid& uuid, const SocketAddress& endpoint)
{
    const auto it = m_modules.emplace(uuid, std::unique_ptr<Module>());
    if (it.second)
        it.first->second = std::make_unique<Module>(this, uuid);

    it.first->second->addEndpoint(endpoint);
}

void ModuleConnector::stopWhileInAioThread()
{
    m_modules.clear();
}

ModuleConnector::Module::Module(ModuleConnector* parent, const QnUuid& uuid):
    m_parent(parent),
    m_uuid(uuid)
{
    m_timer.bindToAioThread(parent->getAioThread());
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
        m_parent->m_disconnectedHandler(m_uuid);
        m_timer.start(kReconnectInterval, [this](){ connect(m_endpoints.begin()); });
        return;
    }

    if (endpointsGroup->second.empty())
        return connect(++endpointsGroup);

    m_timer.cancelSync();
    for (const auto& endpoint: endpointsGroup->second)
    {
        NX_LOGX(lm("Attempt to connect to %1 by %2").strs(m_uuid, endpoint), cl_logDEBUG2);
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
                    for (const auto& client: m_httpClients)
                        client->pleaseStopSync();
                    m_httpClients.clear();

                    m_parent->m_connectedHandler(moduleInformation.get(), endpoint);
                    return monitorConnection(std::move(client));
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
        || moduleInformation.id != m_uuid)
    {
        NX_LOGX(lm("Connected to a wrong server (%2) instead of %3")
            .strs(moduleInformation.id, m_uuid), cl_logDEBUG2);
        return boost::none;
    }

    return moduleInformation;
}

void ModuleConnector::Module::monitorConnection(nx_http::AsyncHttpClientPtr client)
{
    NX_LOGX(lm("Connected to %1").strs(m_uuid), cl_logDEBUG1);

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

} // namespace discovery
} // namespace vms
} // namespace nx
