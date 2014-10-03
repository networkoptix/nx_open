#include "common_message_processor.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_discovery_data.h>

#include <api/app_server_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>

#include <business/business_event_rule.h>

#include <core/resource_management/resource_pool.h>
#include "common/common_module.h"
#include "utils/common/synctime.h"
#include "runtime_info_manager.h"
#include <utils/common/app_info.h>
#include "nx_ec/data/api_resource_data.h"

QnCommonMessageProcessor::QnCommonMessageProcessor(QObject *parent) :
    base_type(parent)
{
}

void QnCommonMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    if (m_connection) {
        /* Safety check in case connection will not be deleted instantly. */
        m_connection->disconnect(this);
        m_connection->getResourceManager()->disconnect(this);
        m_connection->getMediaServerManager()->disconnect(this);
        m_connection->getCameraManager()->disconnect(this);
        m_connection->getLicenseManager()->disconnect(this);
        m_connection->getBusinessEventManager()->disconnect(this);
        m_connection->getUserManager()->disconnect(this);
        m_connection->getLayoutManager()->disconnect(this);
        m_connection->getStoredFileManager()->disconnect(this);
        m_connection->getDiscoveryManager()->disconnect(this);
        m_connection->getTimeManager()->disconnect(this);
    }
    m_connection = connection;

    if (!connection)
        return;

    connect(connection, &ec2::AbstractECConnection::remotePeerFound,                this, &QnCommonMessageProcessor::at_remotePeerFound );
    connect(connection, &ec2::AbstractECConnection::remotePeerLost,                 this, &QnCommonMessageProcessor::at_remotePeerLost );
    connect(connection, &ec2::AbstractECConnection::remotePeerFound,                this, &QnCommonMessageProcessor::remotePeerFound );
    connect(connection, &ec2::AbstractECConnection::remotePeerLost,                 this, &QnCommonMessageProcessor::remotePeerLost );
    connect(connection, &ec2::AbstractECConnection::initNotification,               this, &QnCommonMessageProcessor::on_gotInitialNotification );
    connect(connection, &ec2::AbstractECConnection::runtimeInfoChanged,             this, &QnCommonMessageProcessor::runtimeInfoChanged );
    connect(connection, &ec2::AbstractECConnection::panicModeChanged,               this, &QnCommonMessageProcessor::on_panicModeChanged );

    auto resourceManager = connection->getResourceManager();
    connect(resourceManager, &ec2::AbstractResourceManager::resourceChanged,        this, [this](const QnResourcePtr &resource){updateResource(resource);});
    connect(resourceManager, &ec2::AbstractResourceManager::statusChanged,          this, &QnCommonMessageProcessor::on_resourceStatusChanged );
    connect(resourceManager, &ec2::AbstractResourceManager::resourceParamsChanged,  this, &QnCommonMessageProcessor::on_resourceParamsChanged );
    connect(resourceManager, &ec2::AbstractResourceManager::resourceRemoved,        this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto mediaServerManager = connection->getMediaServerManager();
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::addedOrUpdated,   this, [this](const QnMediaServerResourcePtr &server){updateResource(server);});
    connect(mediaServerManager, &ec2::AbstractMediaServerManager::removed,          this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto cameraManager = connection->getCameraManager();
    connect(cameraManager, &ec2::AbstractCameraManager::cameraAddedOrUpdated,       this, [this](const QnVirtualCameraResourcePtr &camera){updateResource(camera);});
    connect(cameraManager, &ec2::AbstractCameraManager::cameraHistoryChanged,       this, &QnCommonMessageProcessor::on_cameraHistoryChanged );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraHistoryRemoved,       this, &QnCommonMessageProcessor::on_cameraHistoryRemoved );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraRemoved,              this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraBookmarkTagsAdded,    this, &QnCommonMessageProcessor::cameraBookmarkTagsAdded );
    connect(cameraManager, &ec2::AbstractCameraManager::cameraBookmarkTagsRemoved,  this, &QnCommonMessageProcessor::cameraBookmarkTagsRemoved );

    auto userManager = connection->getUserManager();
    connect(userManager, &ec2::AbstractUserManager::addedOrUpdated,                 this, [this](const QnUserResourcePtr &user){updateResource(user);});
    connect(userManager, &ec2::AbstractUserManager::removed,                        this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto layoutManager = connection->getLayoutManager();
    connect(layoutManager, &ec2::AbstractLayoutManager::addedOrUpdated,             this, [this](const QnLayoutResourcePtr &layout){updateResource(layout);});
    connect(layoutManager, &ec2::AbstractLayoutManager::removed,                    this, &QnCommonMessageProcessor::on_resourceRemoved );

    auto videowallManager = connection->getVideowallManager();
    connect(videowallManager, &ec2::AbstractVideowallManager::addedOrUpdated,       this, [this](const QnVideoWallResourcePtr &videowall){updateResource(videowall);});
    connect(videowallManager, &ec2::AbstractVideowallManager::removed,              this, &QnCommonMessageProcessor::on_resourceRemoved );
    connect(videowallManager, &ec2::AbstractVideowallManager::controlMessage,       this, &QnCommonMessageProcessor::videowallControlMessageReceived );

    auto licenseManager = connection->getLicenseManager();
    connect(licenseManager, &ec2::AbstractLicenseManager::licenseChanged,           this, &QnCommonMessageProcessor::on_licenseChanged );
    connect(licenseManager, &ec2::AbstractLicenseManager::licenseRemoved,           this, &QnCommonMessageProcessor::on_licenseRemoved );

    auto eventManager = connection->getBusinessEventManager();
    connect(eventManager, &ec2::AbstractBusinessEventManager::addedOrUpdated,       this, &QnCommonMessageProcessor::on_businessEventAddedOrUpdated );
    connect(eventManager, &ec2::AbstractBusinessEventManager::removed,              this, &QnCommonMessageProcessor::on_businessEventRemoved );
    connect(eventManager, &ec2::AbstractBusinessEventManager::businessActionBroadcasted, this, &QnCommonMessageProcessor::on_businessActionBroadcasted );
    connect(eventManager, &ec2::AbstractBusinessEventManager::businessRuleReset,    this, &QnCommonMessageProcessor::on_businessRuleReset );
    connect(eventManager, &ec2::AbstractBusinessEventManager::gotBroadcastAction,   this, &QnCommonMessageProcessor::on_broadcastBusinessAction );
    connect(eventManager, &ec2::AbstractBusinessEventManager::execBusinessAction,   this, &QnCommonMessageProcessor::on_execBusinessAction );

    auto storedFileManager = connection->getStoredFileManager();
    connect(storedFileManager, &ec2::AbstractStoredFileManager::added,              this, &QnCommonMessageProcessor::fileAdded );
    connect(storedFileManager, &ec2::AbstractStoredFileManager::updated,            this, &QnCommonMessageProcessor::fileUpdated );
    connect(storedFileManager, &ec2::AbstractStoredFileManager::removed,            this, &QnCommonMessageProcessor::fileRemoved );

    auto timeManager = connection->getTimeManager();
    connect(timeManager, &ec2::AbstractTimeManager::timeServerSelectionRequired,    this, &QnCommonMessageProcessor::timeServerSelectionRequired);
    connect(timeManager, &ec2::AbstractTimeManager::timeChanged,                    this, &QnCommonMessageProcessor::syncTimeChanged);
    connect(timeManager, &ec2::AbstractTimeManager::peerTimeChanged,                this, &QnCommonMessageProcessor::peerTimeChanged);

    connect( connection->getDiscoveryManager(), &ec2::AbstractDiscoveryManager::discoveryInformationChanged,
        this, &QnCommonMessageProcessor::on_gotDiscoveryData );

    connection->startReceivingNotifications();
}

/*
* EC2 related processing. Need move to other class
*/

void QnCommonMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data)
{
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res)
        res->setStatus(Qn::Online);

}

void QnCommonMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data)
{
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res) {
        res->setStatus(Qn::Offline);
        if (data.peer.peerType != Qn::PT_Server) {
            // This server hasn't own DB
            foreach(QnResourcePtr camera, qnResPool->getAllCameras(res))
                camera->setStatus(Qn::Offline);
        }
    }
}


void QnCommonMessageProcessor::on_gotInitialNotification(const ec2::QnFullResourceData &fullData)
{
    onGotInitialNotification(fullData);
    on_businessRuleReset(fullData.bRules);
}


void QnCommonMessageProcessor::on_gotDiscoveryData(const ec2::ApiDiscoveryData &data, bool addInformation)
{
    QMultiHash<QnUuid, QUrl> m_additionalUrls;
    QMultiHash<QnUuid, QUrl> m_ignoredUrls;

    QUrl url(data.url);
    if (data.ignore) {
        if (url.port() != -1 && !m_additionalUrls.contains(data.id, url))
            m_additionalUrls.insert(data.id, url);
        m_ignoredUrls.insert(data.id, url);
    } else {
        if (!m_additionalUrls.contains(data.id, url))
            m_additionalUrls.insert(data.id, url);
    }

    foreach (const QnUuid &id, m_additionalUrls.uniqueKeys()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QList<QUrl> additionalUrls = server->getAdditionalUrls();

        if (addInformation) {
            foreach (const QUrl &url, m_additionalUrls.values(id)) {
                if (!additionalUrls.contains(url))
                    additionalUrls.append(url);
            }
        } else {
            foreach (const QUrl &url, m_additionalUrls.values(id))
                additionalUrls.removeOne(url);
        }
        server->setAdditionalUrls(additionalUrls);
    }

    foreach (const QnUuid &id, m_ignoredUrls.uniqueKeys()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QList<QUrl> ignoredUrls = server->getIgnoredUrls();

        if (addInformation) {
            foreach (const QUrl &url, m_additionalUrls.values(id))
                ignoredUrls.removeOne(url);
            foreach (const QUrl &url, m_ignoredUrls.values(id)) {
                if (!ignoredUrls.contains(url))
                    ignoredUrls.append(url);
            }
        } else {
            foreach (const QUrl &url, m_ignoredUrls.values(id))
                ignoredUrls.removeOne(url);
        }
        server->setIgnoredUrls(ignoredUrls);
    }
}

void QnCommonMessageProcessor::on_resourceStatusChanged( const QnUuid& resourceId, Qn::ResourceStatus status )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (resource)
        onResourceStatusChanged(resource, status);
}

void QnCommonMessageProcessor::on_resourceParamsChanged( const QnUuid& resourceId, const ec2::ApiResourceParamDataList& kvPairs )
{
    QnResourcePtr resource = qnResPool->getResourceById(resourceId);
    if (!resource)
        return;

    foreach (const ec2::ApiResourceParamData &pair, kvPairs)
        resource->setProperty(pair.name, pair.value);
}

void QnCommonMessageProcessor::on_resourceRemoved( const QnUuid& resourceId )
{
    if (canRemoveResource(resourceId))
    {
        //beforeRemovingResource(resourceId);

        if (QnResourcePtr ownResource = qnResPool->getResourceById(resourceId)) 
        {
            // delete dependent objects
            foreach(QnResourcePtr subRes, qnResPool->getResourcesByParentId(resourceId))
                qnResPool->removeResource(subRes);
            qnResPool->removeResource(ownResource);
        }
    
        afterRemovingResource(resourceId);
    }
    else
        removeResourceIgnored(resourceId);
}

void QnCommonMessageProcessor::on_cameraHistoryChanged(const QnCameraHistoryItemPtr &cameraHistory) {
    QnCameraHistoryPool::instance()->addCameraHistoryItem(*cameraHistory.data());
}

void QnCommonMessageProcessor::on_cameraHistoryRemoved(const QnCameraHistoryItemPtr &cameraHistory) {
    QnCameraHistoryPool::instance()->removeCameraHistoryItem(*cameraHistory.data());
}

void QnCommonMessageProcessor::on_licenseChanged(const QnLicensePtr &license) {
    qnLicensePool->addLicense(license);
}

void QnCommonMessageProcessor::on_licenseRemoved(const QnLicensePtr &license) {
    qnLicensePool->removeLicense(license);
}

void QnCommonMessageProcessor::on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &businessRule){
    m_rules[businessRule->id()] = businessRule;
    emit businessRuleChanged(businessRule);
}

void QnCommonMessageProcessor::on_businessEventRemoved(const QnUuid &id) {
    m_rules.remove(id);
    emit businessRuleDeleted(id);
}

void QnCommonMessageProcessor::on_businessActionBroadcasted( const QnAbstractBusinessActionPtr& /* businessAction */ )
{
    // nothing to do for a while
}

void QnCommonMessageProcessor::on_businessRuleReset( const QnBusinessEventRuleList& rules )
{
    m_rules.clear();
    foreach(const QnBusinessEventRulePtr &bRule, rules)
        m_rules[bRule->id()] = bRule;

    emit businessRuleReset(rules);
}

void QnCommonMessageProcessor::on_broadcastBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    emit businessActionReceived(action);
}

void QnCommonMessageProcessor::on_execBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    execBusinessActionInternal(action);
}

void QnCommonMessageProcessor::on_panicModeChanged(Qn::PanicMode mode) {
    QnResourceList resList = qnResPool->getAllResourceByTypeName(lit("Server"));
    foreach(QnResourcePtr res, resList) {
        QnMediaServerResourcePtr mServer = res.dynamicCast<QnMediaServerResource>();
        if (mServer)
            mServer->setPanicMode(mode);
    }
}

// todo: ec2 relate logic. remove from this class
void QnCommonMessageProcessor::afterRemovingResource(const QnUuid& id) {
    foreach(QnBusinessEventRulePtr bRule, m_rules.values())
    {
        if (bRule->eventResources().contains(id) || bRule->actionResources().contains(id))
        {
            QnBusinessEventRulePtr updatedRule(bRule->clone());
            updatedRule->removeResource(id);
            emit businessRuleChanged(updatedRule);
        }
    }
}


void QnCommonMessageProcessor::processResources(const QnResourceList& resources)
{
    qnResPool->beginTran();
    foreach (const QnResourcePtr& resource, resources)
        updateResource(resource);
    qnResPool->commit();
}

void QnCommonMessageProcessor::processLicenses(const QnLicenseList& licenses)
{
    qnLicensePool->replaceLicenses(licenses);
}

void QnCommonMessageProcessor::processCameraServerItems(const QnCameraHistoryList& cameraHistoryList)
{
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
}

bool QnCommonMessageProcessor::canRemoveResource(const QnUuid &) 
{ 
    return true; 
}

void QnCommonMessageProcessor::removeResourceIgnored(const QnUuid &) 
{
}

void QnCommonMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    //QnAppServerConnectionFactory::setBox(fullData.serverInfo.platform);

    processResources(fullData.resources);
    processLicenses(fullData.licenses);
    processCameraServerItems(fullData.cameraHistory);
    //on_runtimeInfoChanged(fullData.serverInfo);
    qnSyncTime->reset();   

    if (!m_connection)
        return;

    auto timeManager = m_connection->getTimeManager();
    timeManager->getCurrentTime(this, [this](int handle, ec2::ErrorCode errCode, qint64 syncTime) {
        Q_UNUSED(handle);
        if (errCode != ec2::ErrorCode::ok || !m_connection)
            return;

        emit syncTimeChanged(syncTime);

        ec2::QnPeerTimeInfoList peers = m_connection->getTimeManager()->getPeerTimeInfoList();
        foreach(const ec2::QnPeerTimeInfo &info, peers) {
            if( !QnRuntimeInfoManager::instance()->hasItem(info.peerId) ) {
                qWarning() << "Time for peer" << info.peerId << "received before peer was found";
                continue;
            }
            Q_ASSERT(QnRuntimeInfoManager::instance()->item(info.peerId).data.peer.peerType == Qn::PT_Server);
            emit peerTimeChanged(info.peerId, syncTime, info.time);
        }
    });

    emit initialResourcesReceived();
}

QMap<QnUuid, QnBusinessEventRulePtr> QnCommonMessageProcessor::businessRules() const {
    return m_rules;
}

void QnCommonMessageProcessor::updateResource(const QnResourcePtr &resource) 
{
#if 0
    if (dynamic_cast<const QnMediaServerResource*>(resource.data()))
    {
        if (QnRuntimeInfoManager::instance()->hasItem(resource->getId()))
            resource->setStatus(Qn::Online);
        else
            resource->setStatus(Qn::Offline);
    }
#endif
    if (resource->getId() == qnCommon->moduleGUID()) {
        QnModuleInformation moduleInformation = qnCommon->moduleInformation();
        moduleInformation.name = resource->getName();
        qnCommon->setModuleInformation(moduleInformation);
    }
}
