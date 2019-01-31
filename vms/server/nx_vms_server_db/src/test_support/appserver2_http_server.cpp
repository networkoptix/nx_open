#include "appserver2_http_server.h"

#include <nx/network/http/custom_headers.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>

#include "ec2_connection_processor.h"
#include "transaction/message_bus_adapter.h"
#include <core/resource_management/status_dictionary.h>

namespace ec2 {

JsonConnectionProcessor::JsonConnectionProcessor(
    ProcessorHandler handler,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnHttpConnectionListener* owner)
    :
    QnTCPConnectionProcessor(std::move(socket), owner),
    m_handler(handler)
{
}

void JsonConnectionProcessor::run()
{
    QnJsonRestResult result;
    auto owner = static_cast<QnHttpConnectionListener*>(d_ptr->owner);
    const auto httpStatusCode = m_handler(d_ptr->request, owner, &result);
    d_ptr->response.messageBody = QJson::serialized(result);
    sendResponse(
        httpStatusCode,
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat));
}

//-------------------------------------------------------------------------------------------------

QnSimpleHttpConnectionListener::QnSimpleHttpConnectionListener(
    QnCommonModule* commonModule,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
    :
    QnHttpConnectionListener(commonModule, address, port, maxConnections, useSsl)
{
}

QnSimpleHttpConnectionListener::~QnSimpleHttpConnectionListener()
{
    stop();
}

QnTCPConnectionProcessor* QnSimpleHttpConnectionListener::createRequestProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket)
{
    return new Ec2ConnectionProcessor(
        std::move(clientSocket),
        m_userDataProvider,
        m_nonceProvider,
        this);
}

bool QnSimpleHttpConnectionListener::needAuth(const nx::network::http::Request& request) const
{
    auto path = request.requestLine.url.path();
    for (const auto& value : m_disableAuthPrefixes)
    {
        if (path.contains(value))
            return false;
    }
    QUrlQuery query(request.requestLine.url.query());
    if (query.hasQueryItem(Qn::URL_QUERY_AUTH_KEY_NAME))
        return false; //< Authenticated by url query
    return QnHttpConnectionListener::needAuth();
}

void QnSimpleHttpConnectionListener::disableAuthForPath(const QString& path)
{
    m_disableAuthPrefixes.insert(path);
}

void QnSimpleHttpConnectionListener::setAuthenticator(
    nx::vms::auth::AbstractUserDataProvider* userDataProvider,
    nx::vms::auth::AbstractNonceProvider* nonceProvider)
{
    m_userDataProvider = userDataProvider;
    m_nonceProvider = nonceProvider;
}

//-------------------------------------------------------------------------------------------------

Appserver2MessageProcessor::Appserver2MessageProcessor(QObject* parent):
    QnCommonMessageProcessor(parent),
    m_factory(new nx::TestResourceFactory())
{
    connect(resourcePool(), &QnResourcePool::statusChanged, this,
        [this](const QnResourcePtr& resource)
        {
            const QnMediaServerResourcePtr& mserverRes = resource.dynamicCast<QnMediaServerResource>();
            if (!mserverRes)
                return;

            auto connection = commonModule()->ec2Connection();
            auto manager = connection->getResourceManager(Qn::kSystemAccess);
            manager->setResourceStatusSync(resource->getId(), resource->getStatus());
        });
}

void Appserver2MessageProcessor::onResourceStatusChanged(
    const QnResourcePtr& resource,
    Qn::ResourceStatus status,
    ec2::NotificationSource /*source*/)
{
    commonModule()->resourceStatusDictionary()->setValue(resource->getId(), status);
}

QnResourceFactory* Appserver2MessageProcessor::getResourceFactory() const
{
    return m_factory.get();
}

bool Appserver2MessageProcessor::canRemoveResource(const QnUuid& id)
{
    return id != commonModule()->moduleGUID();
}

void Appserver2MessageProcessor::removeResourceIgnored(const QnUuid& resourceId)
{
    if (resourceId != commonModule()->moduleGUID())
        return;
    QnMediaServerResourcePtr mServer = resourcePool()->getResourceById<QnMediaServerResource>(resourceId);

    nx::vms::api::MediaServerData apiServer;
    ec2::fromResourceToApi(mServer, apiServer);
    auto connection = commonModule()->ec2Connection();
    connection->getMediaServerManager(Qn::kSystemAccess)->save(
        apiServer,
        ec2::DummyHandler::instance(),
        &ec2::DummyHandler::onRequestDone);
}

void Appserver2MessageProcessor::updateResource(
    const QnResourcePtr& resource,
    ec2::NotificationSource /*source*/)
{
    commonModule()->resourcePool()->addResource(resource);
    if (m_delayedOnlineStatus.contains(resource->getId()))
    {
        m_delayedOnlineStatus.remove(resource->getId());
        resource->setStatus(Qn::Online);
    }
}

void Appserver2MessageProcessor::handleRemotePeerFound(
    QnUuid peer, nx::vms::api::PeerType peerType)
{
    base_type::handleRemotePeerFound(peer, peerType);
    QnResourcePtr res = resourcePool()->getResourceById(peer);
    if (res)
        res->setStatus(Qn::Online);
    else
        m_delayedOnlineStatus << peer;
}

void Appserver2MessageProcessor::handleRemotePeerLost(
    QnUuid peer, nx::vms::api::PeerType peerType)
{
    base_type::handleRemotePeerLost(peer, peerType);
    QnResourcePtr res = resourcePool()->getResourceById(peer);
    if (res)
        res->setStatus(Qn::Offline);
    m_delayedOnlineStatus.remove(peer);
}

} // namespace ec2
