#include "module_connector.h"

#include <rest/server/json_rest_result.h>
#include <nx/network/socket_global.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/network/address_resolver.h>
#include <nx/network/url/url_builder.h>
#include <rest/server/json_rest_result.h>

#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace discovery {

static const nx::utils::Url kUrl(lit(
    "http://localhost/api/moduleInformation?showAddresses=false&keepConnectionOpen&updateStream"));

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

ModuleConnector::InformationReader::InformationReader(const ModuleConnector* parent):
    m_parent(parent),
    m_httpClient(nx_http::AsyncHttpClient::create())
{
    m_httpClient->bindToAioThread(parent->getAioThread());
}

ModuleConnector::InformationReader::~InformationReader()
{
    if (m_httpClient)
        m_httpClient->pleaseStopSync();
}

void ModuleConnector::InformationReader::setHandler(
    std::function<void(boost::optional<QnModuleInformation>, QString)> handler)
{
    m_handler = std::move(handler);
}

void ModuleConnector::InformationReader::start(const SocketAddress& endpoint)
{
    const auto handler =
        [this](nx_http::AsyncHttpClientPtr client) mutable
        {
            NX_ASSERT(m_httpClient, client);
            const auto clientGuard = makeScopeGuard([client](){ client->pleaseStopSync(); });
            m_httpClient.reset();
            if (!client->hasRequestSucceeded())
                return nx::utils::swapAndCall(m_handler, boost::none, lit("HTTP request has failed"));

            m_buffer = client->fetchMessageBodyBuffer();
            m_socket = client->takeSocket();

            if (!m_socket->setRecvTimeout(m_parent->m_disconnectTimeout))
                return nx::utils::swapAndCall(m_handler, boost::none, SystemError::getLastOSErrorText());

            readUntilError();
        };

    QObject::connect(m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, handler);
    QObject::connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, handler);
    m_httpClient->doGet(nx::network::url::Builder(kUrl).setEndpoint(endpoint));
}

static inline boost::optional<nx::Buffer> takeJsonObject(nx::Buffer* buffer)
{
    size_t bracerCount = 0;
    for (int index = 0; index < buffer->size(); ++index)
    {
        switch (buffer->at(index))
        {
            case '{': ++bracerCount; break;
            case '}': --bracerCount; break;
        }

        if (bracerCount == 0 && index != 0)
        {
            const auto object = buffer->left(index + 1);
            *buffer = buffer->mid(index + 1);
            return object;
        }
    }

    return boost::none;
}

void ModuleConnector::InformationReader::readUntilError()
{
    while (const auto object = takeJsonObject(&m_buffer))
    {
        static const auto kEmptyObject = QJson::serialize(QJsonObject());
        if (*object == kEmptyObject)
            continue;

        QnJsonRestResult restResult;
        if (!QJson::deserialize(*object, &restResult)
            || restResult.error != QnRestResult::Error::NoError)
        {
            return nx::utils::swapAndCall(m_handler, boost::none, restResult.errorString);
        }

        QnModuleInformation moduleInformation;
        if (!QJson::deserialize<QnModuleInformation>(restResult.reply, &moduleInformation)
            || moduleInformation.id.isNull())
        {
            return nx::utils::swapAndCall(m_handler, boost::none, restResult.errorString);
        }

        nx::utils::ObjectDestructionFlag::Watcher destructionWatcher(&m_destructionFlag);
        const auto localHandler = m_handler;
        localHandler(std::move(moduleInformation), QString());
        if (destructionWatcher.objectDestroyed())
            return;
    }

    m_buffer.reserve(1500);
    m_socket->readSomeAsync(&m_buffer,
        [this](SystemError::ErrorCode code, size_t size)
        {
            if (code != SystemError::noError)
                return nx::utils::swapAndCall(m_handler, boost::none, SystemError::toString(code));

            if (size == 0)
                return nx::utils::swapAndCall(m_handler, boost::none, lit("Disconnected"));

            readUntilError();
        });
}

ModuleConnector::Module::Module(ModuleConnector* parent, const QnUuid& id):
    m_parent(parent),
    m_id(id),
    m_reconnectTimer(parent->m_retryPolicy, parent->getAioThread()),
    m_disconnectTimer(parent->getAioThread())
{
    NX_DEBUG(this) << "New";
}

ModuleConnector::Module::~Module()
{
    NX_DEBUG(this) << "Delete";
    NX_ASSERT(m_reconnectTimer.isInSelfAioThread());
    m_attemptingReaders.clear();
    m_connectedReader.reset();
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
    if ((m_id.isNull()) || (m_attemptingReaders.empty() && !m_connectedReader))
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

    if (host.ipV4() || (bool) host.ipV6().first)
        return kIp; //< TODO: Consider to check if we have such interface.

    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(host.toString()))
        return kCloud;

    return kOther;
}

QString ModuleConnector::Module::idForToStringFromPtr() const
{
    return m_id.toSimpleString();
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

        NX_VERBOSE(this, "Refuse to connect in passive mode");
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

    if (m_connectedReader)
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
        NX_ASSERT(!endpoint.toString().isEmpty());
        connectToEndpoint(endpoint, endpointsGroup);
    }

    // Go to a next group if we could not start any connects in current group.
    if (endpointsInProgress == 0)
        return connectToGroup(++endpointsGroup);
}

void ModuleConnector::Module::connectToEndpoint(
    const SocketAddress& endpoint, Endpoints::iterator endpointsGroup)
{
    NX_VERBOSE(this, lm("Attempt to connect by %1").arg(endpoint));
    m_attemptingReaders.push_front(std::make_unique<InformationReader>(m_parent));

    const auto readerIt = m_attemptingReaders.begin();
    (*readerIt)->start(endpoint);
    (*readerIt)->setHandler(
        [this, endpoint, endpointsGroup, readerIt](
            boost::optional<QnModuleInformation> information, QString description) mutable
       {
           std::unique_ptr<InformationReader> reader(std::move(*readerIt));
           m_attemptingReaders.erase(readerIt);

           if (information)
           {
                if (information->id == m_id)
                {
                    if (saveConnection(std::move(endpoint), std::move(reader), *information))
                        return;
                }
                else
                {
                    endpointsGroup->second.erase(endpoint); //< Wrong endpoint.
                    m_parent->getModule(information->id)->saveConnection(
                        std::move(endpoint), std::move(reader), *information);
                }
           }

           if (m_id.isNull())
           {
               endpointsGroup->second.erase(endpoint);
               return;
           }

           NX_VERBOSE(this, lm("Could not connect by %1: %2").args(endpoint, description));

           // When the last endpoint in a group fails try the next group.
           if (m_attemptingReaders.empty())
               connectToGroup(std::next(endpointsGroup));
       });
}

bool ModuleConnector::Module::saveConnection(SocketAddress endpoint,
    std::unique_ptr<InformationReader> reader, const QnModuleInformation& information)
{
    NX_ASSERT(!m_id.isNull());
    if (m_id.isNull())
        return false;

    saveEndpoint(endpoint);
    if (m_connectedReader)
        return true;

    NX_VERBOSE(this, lm("Save connection to %1").args(endpoint));
    m_attemptingReaders.clear();
    m_disconnectTimer.cancelSync();

    m_connectedReader = std::move(reader);
    m_connectedReader->setHandler(
        [this, endpoint](boost::optional<QnModuleInformation> information, QString description) mutable
        {
            if (information)
            {
                NX_VERBOSE(this, lm("Module information update from %1").arg(endpoint));
                return m_parent->m_connectedHandler(*information, endpoint, m_connectedReader->ip());
            }

            NX_VERBOSE(this, lm("Connection to %1 is closed: %2").args(endpoint, description));
            m_connectedReader.reset();
            ensureConnection(); //< Reconnect attempt.

            const auto reconnectTimeout = m_parent->m_disconnectTimeout
                * std::chrono::milliseconds::rep(m_endpoints.size());
            m_disconnectTimer.start(reconnectTimeout,
                [this, reconnectTimeout]()
                {
                    NX_VERBOSE(this, lm("Reconnect did not happen in %1").arg(reconnectTimeout));
                    m_parent->m_disconnectedHandler(m_id);
                });
        });

    m_parent->m_connectedHandler(information, std::move(endpoint), m_connectedReader->ip());
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
