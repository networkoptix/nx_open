#include "client_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource_management/resource_discovery_manager.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"
#include "device_plugins/server_camera/server_camera.h"
#include "utils/incompatible_server_adder.h"
#include "utils/network/global_module_finder.h"

#include "version.h"

QnClientMessageProcessor::QnClientMessageProcessor():
    base_type(),
    m_opened(false),
    m_incompatibleServerAdder(NULL)
{
}

void QnClientMessageProcessor::init(ec2::AbstractECConnectionPtr connection)
{
    QnCommonMessageProcessor::init(connection);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnClientMessageProcessor::at_remotePeerFound);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnClientMessageProcessor::at_remotePeerLost);

    connect( connection->getMiscManager().get(), &ec2::AbstractMiscManager::systemNameChangeRequested,
             this, &QnClientMessageProcessor::at_systemNameChangeRequested );
}

void QnClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) {
    resource->setStatus(status);
    checkForTmpStatus(resource);
}

void QnClientMessageProcessor::updateResource(const QnResourcePtr &resource) {
    QnResourcePtr ownResource;

    ownResource = qnResPool->getIncompatibleResourceById(resource->getId(), true);

    // Use discovery information to update offline servers. They may be just incompatible.
    if (resource->getStatus() == QnResource::Offline) {
        QnModuleInformation moduleInformation = QnGlobalModuleFinder::instance()->moduleInformation(resource->getId());
        if (!moduleInformation.id.isNull()) {
            if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>()) {
                mediaServer->setVersion(moduleInformation.version);
                mediaServer->setSystemInfo(moduleInformation.systemInformation);
                mediaServer->setSystemName(moduleInformation.systemName);
                if (moduleInformation.systemName != qnCommon->localSystemName() || moduleInformation.version != qnCommon->engineVersion())
                    mediaServer->setStatus(QnResource::Incompatible);
            }
        }
    }

    if (ownResource.isNull()) {
        qnResPool->addResource(resource);
        if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>())
            determineOptimalIF(mediaServer);
    }
    else {
        bool mserverStatusChanged = false;
        bool compatibleStatusChanged = false;
        QnMediaServerResourcePtr mediaServer = ownResource.dynamicCast<QnMediaServerResource>();
        if (mediaServer) {
            mserverStatusChanged = ownResource->getStatus() != resource->getStatus();
            compatibleStatusChanged = mserverStatusChanged && (ownResource->getStatus() == QnResource::Incompatible || resource->getStatus() == QnResource::Incompatible);
        }

        // move incompatible resource to the main pool if it became normal
        if (ownResource && ownResource->getStatus() == QnResource::Incompatible && resource->getStatus() != QnResource::Incompatible)
            qnResPool->makeResourceNormal(ownResource);

        ownResource->update(resource);

        if (mserverStatusChanged && mediaServer)
            determineOptimalIF(mediaServer);

        // move server into the other subtree if compatibility has been changed
        if (compatibleStatusChanged && mediaServer)
            mediaServer->parentIdChanged(mediaServer);
    }

    // TODO: #Elric #2.3 don't update layout if we're re-reading resources, 
    // this leads to unsaved layouts spontaneously rolling back to last saved state.

    if (QnLayoutResourcePtr layout = ownResource.dynamicCast<QnLayoutResource>())
        layout->requestStore();

    checkForTmpStatus(resource);
}

void QnClientMessageProcessor::processResources(const QnResourceList& resources)
{
    QnCommonMessageProcessor::processResources(resources);
    foreach(const QnResourcePtr& resource, resources)
        checkForTmpStatus(resource);
}

void QnClientMessageProcessor::checkForTmpStatus(QnResourcePtr resource)
{
    // process tmp status
    if (QnMediaServerResourcePtr mediaServer = resource.dynamicCast<QnMediaServerResource>()) 
    {
        if (mediaServer->getStatus() == QnResource::Offline)
            updateServerTmpStatus(mediaServer->getId(), QnResource::Offline);
        else
            updateServerTmpStatus(mediaServer->getId(), QnResource::NotDefined);
    }
    else if (QnServerCameraPtr serverCamera = resource.dynamicCast<QnServerCamera>()) 
    {
        QnMediaServerResourcePtr mediaServer = qnResPool->getResourceById(serverCamera->getParentId()).dynamicCast<QnMediaServerResource>();
        if (mediaServer) {
            if (mediaServer->getStatus() ==QnResource::Offline)
                serverCamera->setTmpStatus(QnResource::Offline);
            else
                serverCamera->setTmpStatus(QnResource::NotDefined);
        }
    }
}

void QnClientMessageProcessor::determineOptimalIF(const QnMediaServerResourcePtr &resource)
{
    // set proxy. If some media server IF will be found, proxy address will be cleared
    QString url = QnAppServerConnectionFactory::defaultUrl().host();
    if (url.isEmpty())
        url = QLatin1String("127.0.0.1");
    int port = QnAppServerConnectionFactory::defaultMediaProxyPort();
    resource->apiConnection()->setProxyAddr(resource->getApiUrl(), url, port);
    disconnect(resource.data(), NULL, this, NULL);
    //connect(resource.data(), SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)), 
    //         this, SLOT(at_serverIfFound(const QnMediaServerResourcePtr &, const QString &, const QString &)));
    resource->determineOptimalNetIF();
}

void QnClientMessageProcessor::updateServerTmpStatus(const QnId& id, QnResource::Status status)
{
    QnResourcePtr server = qnResPool->getResourceById(id);
    if (!server)
        return;
    foreach(QnResourcePtr res, qnResPool->getAllCameras(server)) {
        QnServerCameraPtr serverCamera = res.dynamicCast<QnServerCamera>();
        if (serverCamera)
            serverCamera->setTmpStatus(status);
    }
}

void QnClientMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy)
{
    if (data.peerId == qnCommon->moduleGUID())
        return;

    if (isProxy) {
        //updateTmpStatus(id, QnResource::NotDefined);
        return;
    }

    if (!m_opened) {
        m_opened = true;
        emit connectionOpened();
    }
}

void QnClientMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData, bool isProxy)
{
    if (isProxy) {
        //updateTmpStatus(id, QnResource::Offline);
        return;
    }

    if (m_opened) {
        m_opened = false;
        emit connectionClosed();
        QString serverTypeName = lit("Server");
        foreach(QnResourcePtr res, qnResPool->getAllResourceByTypeName(serverTypeName))
            res->setStatus(QnResource::Offline);
        foreach(QnResourcePtr res, qnResPool->getAllCameras(QnResourcePtr()))
            res->setStatus(QnResource::Offline);
    }
}

void QnClientMessageProcessor::at_systemNameChangeRequested(const QString &systemName) {
    if (qnCommon->localSystemName() == systemName)
        return;

    qnCommon->setLocalSystemName(systemName);
}

void QnClientMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    if (m_incompatibleServerAdder) {
        delete m_incompatibleServerAdder;
        m_incompatibleServerAdder = 0;
    }

    QnCommonMessageProcessor::onGotInitialNotification(fullData);
    QnResourceDiscoveryManager::instance()->setReady(true);
    //emit connectionOpened();

    m_incompatibleServerAdder = new QnIncompatibleServerAdder(this);
}
