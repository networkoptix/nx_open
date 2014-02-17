
#include "ec2_message_source.h"

#include "core/resource/camera_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource/user_resource.h"


QnMessageSource2::QnMessageSource2(ec2::AbstractECConnectionPtr connection)
{
    connect( connection.get(), &ec2::AbstractECConnection::initNotification,
             this, &QnMessageSource2::on_gotInitialNotification );
    connect( connection.get(), &ec2::AbstractECConnection::runtimeInfoChanged,
             this, &QnMessageSource2::on_runtimeInfoChanged );

    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::statusChanged,
             this, &QnMessageSource2::on_resourceStatusChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::disabledChanged,
             this, &QnMessageSource2::on_resourceDisabledChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceChanged,
             this, &QnMessageSource2::on_resourceChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceParamsChanged,
             this, &QnMessageSource2::on_resourceParamsChanged );
    connect( connection->getResourceManager().get(), &ec2::AbstractResourceManager::resourceRemoved,
             this, &QnMessageSource2::on_resourceRemoved );

    connect( connection->getMediaServerManager().get(), &ec2::AbstractMediaServerManager::addedOrUpdated,
             this, &QnMessageSource2::on_mediaServerAddedOrUpdated );
    connect( connection->getMediaServerManager().get(), &ec2::AbstractMediaServerManager::removed,
             this, &QnMessageSource2::on_mediaServerRemoved );

    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraAddedOrUpdated,
             this, &QnMessageSource2::on_cameraAddedOrUpdated );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraHistoryChanged,
             this, &QnMessageSource2::on_cameraHistoryChanged );
    connect( connection->getCameraManager().get(), &ec2::AbstractCameraManager::cameraRemoved,
             this, &QnMessageSource2::on_cameraRemoved );

    connect( connection->getLicenseManager().get(), &ec2::AbstractLicenseManager::licenseChanged,
             this, &QnMessageSource2::on_licenseChanged );

    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::addedOrUpdated,
             this, &QnMessageSource2::on_businessEventAddedOrUpdated );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::removed,
             this, &QnMessageSource2::on_businessEventRemoved );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::businessActionBroadcasted,
             this, &QnMessageSource2::on_businessActionBroadcasted );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::businessRuleReset,
             this, &QnMessageSource2::on_businessRuleReset );
    connect( connection->getBusinessEventManager().get(), &ec2::AbstractBusinessEventManager::gotBroadcastAction,
             this, &QnMessageSource2::on_broadcastBusinessAction );

    connect( connection->getUserManager().get(), &ec2::AbstractUserManager::addedOrUpdated,
             this, &QnMessageSource2::on_userAddedOrUpdated );
    connect( connection->getUserManager().get(), &ec2::AbstractUserManager::removed,
             this, &QnMessageSource2::on_userRemoved );

    connect( connection->getLayoutManager().get(), &ec2::AbstractLayoutManager::addedOrUpdated,
             this, &QnMessageSource2::on_layoutAddedOrUpdated );
    connect( connection->getLayoutManager().get(), &ec2::AbstractLayoutManager::removed,
             this, &QnMessageSource2::on_layoutRemoved );

    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::added,
             this, &QnMessageSource2::on_storedFileAdded );
    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::updated,
             this, &QnMessageSource2::on_storedFileUpdated );
    connect( connection->getStoredFileManager().get(), &ec2::AbstractStoredFileManager::removed,
             this, &QnMessageSource2::on_storedFileRemoved );

    connection->startReceivingNotifications(true);
}

void QnMessageSource2::on_gotInitialNotification(ec2::QnFullResourceData fullData)
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_Initial;
    message.resourceTypes = fullData.resTypes;
    message.resources = fullData.resources;
    message.licenses = fullData.licenses;
    message.cameraServerItems = fullData.cameraHistory;
    message.businessRules = fullData.bRules;

    emit connectionOpened(message);
    emit messageReceived(message);
}

void QnMessageSource2::on_runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_RuntimeInfoChange;
    message.publicIp = runtimeInfo.publicIp;
    message.systemName = runtimeInfo.systemName;
    message.sessionKey = runtimeInfo.sessionKey;
    message.allowCameraChanges = runtimeInfo.allowCameraChanges;

    emit messageReceived(message);
}

void QnMessageSource2::on_resourceStatusChanged( const QnId& resourceId, QnResource::Status status )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_ResourceStatusChange;
    message.resourceId = resourceId;
    //message.resourceGuid = ;  TODO
    message.resourceStatus = status;

    emit messageReceived(message);
}

void QnMessageSource2::on_resourceDisabledChanged( const QnId& resourceId, bool disabled )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_ResourceDisabledChange;
    message.resourceId = resourceId;
    //message.resourceGuid = ;  TODO
    message.resourceDisabled = disabled;

    emit messageReceived(message);
}

void QnMessageSource2::on_resourceChanged( const QnResourcePtr& resource )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_ResourceChange;
    message.resource = resource;

    emit messageReceived(message);
}

void QnMessageSource2::on_resourceParamsChanged( const QnId& resourceId, const QnKvPairList& kvPairs )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_KvPairChange;
    message.kvPairs[resourceId] = kvPairs;

    emit messageReceived(message);
}

void QnMessageSource2::on_resourceRemoved( const QnId& resourceId )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_ResourceDelete;
    message.resourceId = resourceId;
    //TODO message.resourceGuid = ;

    emit messageReceived(message);
}

void QnMessageSource2::on_mediaServerAddedOrUpdated( QnMediaServerResourcePtr mediaServer )
{
    on_resourceChanged( mediaServer );
}

void QnMessageSource2::on_mediaServerRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnMessageSource2::on_cameraAddedOrUpdated( QnVirtualCameraResourcePtr camera )
{
    on_resourceChanged( camera );
}

void QnMessageSource2::on_cameraHistoryChanged( QnCameraHistoryItemPtr cameraHistory )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_CameraServerItem;
    message.cameraServerItem = cameraHistory;

    emit messageReceived(message);
}

void QnMessageSource2::on_cameraRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnMessageSource2::on_licenseChanged(QnLicensePtr license)
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_License;
    //TODO/IMPL see parseLicense
    message.license = license;

    emit messageReceived(message);
}

void QnMessageSource2::on_businessEventAddedOrUpdated( QnBusinessEventRulePtr _businessRule )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_BusinessRuleInsertOrUpdate;
    message.businessRule = _businessRule;

    emit messageReceived(message);
}

void QnMessageSource2::on_businessEventRemoved( QnId id )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_BusinessRuleDelete;
    message.resourceId = id;

    emit messageReceived(message);
}

void QnMessageSource2::on_businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_BroadcastBusinessAction;
    message.businessAction = businessAction;

    emit messageReceived(message);
}

void QnMessageSource2::on_businessRuleReset( const QnBusinessEventRuleList& rules )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_BusinessRuleReset;
    message.businessRules = rules;

    emit messageReceived(message);
}

void QnMessageSource2::on_broadcastBusinessAction( const QnAbstractBusinessActionPtr& action )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_BroadcastBusinessAction;
    message.businessAction = action;

    emit messageReceived(message);
}

void QnMessageSource2::on_userAddedOrUpdated( QnUserResourcePtr user )
{
    on_resourceChanged( user );
}

void QnMessageSource2::on_userRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnMessageSource2::on_layoutAddedOrUpdated( QnLayoutResourcePtr layout )
{
    on_resourceChanged( layout );
}

void QnMessageSource2::on_layoutRemoved( QnId id )
{
    on_resourceRemoved( id );
}

void QnMessageSource2::on_storedFileAdded( QString filename )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_FileAdd;
    message.filename = filename;

    emit messageReceived(message);
}

void QnMessageSource2::on_storedFileUpdated( QString filename )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_FileUpdate;
    message.filename = filename;

    emit messageReceived(message);
}

void QnMessageSource2::on_storedFileRemoved( QString filename )
{
    QnMessage message;
    message.seqNumber = ++m_seqNumber;
    message.messageType = Qn::Message_Type_FileRemove;
    message.filename = filename;

    emit messageReceived(message);
}
