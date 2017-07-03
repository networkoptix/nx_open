#pragma once

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <business/business_fwd.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_peer_alive_data.h"
#include "nx_ec/data/api_runtime_data.h"

#include <nx/utils/singleton.h>
#include <utils/common/connective.h>
#include <common/common_module_aware.h>

class QnResourceFactory;

class QnCommonMessageProcessor: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit QnCommonMessageProcessor(QObject* parent = nullptr);
    virtual ~QnCommonMessageProcessor() {}

    virtual void init(const ec2::AbstractECConnectionPtr& connection);

    ec2::AbstractECConnectionPtr connection() const;

    /**
     * @param resource resource to update
     * @param peerId peer what modified resource.
     */
    virtual void updateResource(const QnResourcePtr &resource, ec2::NotificationSource source);

    virtual void updateResource(const ec2::ApiUserData& user, ec2::NotificationSource source);
    virtual void updateResource(const ec2::ApiLayoutData& layout, ec2::NotificationSource source);
    virtual void updateResource(const ec2::ApiVideowallData& videowall, ec2::NotificationSource source);
    virtual void updateResource(const ec2::ApiWebPageData& webpage, ec2::NotificationSource source);
    virtual void updateResource(const ec2::ApiCameraData& camera, ec2::NotificationSource source);
    virtual void updateResource(const ec2::ApiMediaServerData& server, ec2::NotificationSource source);
    virtual void updateResource(const ec2::ApiStorageData& storage, ec2::NotificationSource source);

    void resetServerUserAttributesList( const ec2::ApiMediaServerUserAttributesDataList& serverUserAttributesList );
    void resetCameraUserAttributesList( const ec2::ApiCameraAttributesDataList& cameraUserAttributesList );
    void resetPropertyList(const ec2::ApiResourceParamWithRefDataList& params);
    void resetStatusList(const ec2::ApiResourceStatusDataList& params);
    void resetAccessRights(const ec2::ApiAccessRightsDataList& accessRights);
    void resetUserRoles(const ec2::ApiUserRoleDataList& roles);

signals:
    void connectionOpened();
    void connectionClosed();
    void connectionReset( QnCommonMessageProcessor* );

    void initialResourcesReceived();

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessActionReceived(const QnAbstractBusinessActionPtr& action);
    void execBusinessAction(const QnAbstractBusinessActionPtr& action);

    void videowallControlMessageReceived(const ec2::ApiVideowallControlMessageData& message);

    void runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo);
    void remotePeerFound(QnUuid data, Qn::PeerType peerType);
    void remotePeerLost(QnUuid data, Qn::PeerType peerType);

    void syncTimeChanged(qint64 syncTime);
    void timeServerSelectionRequired();

    void discoveredServerChanged(const ec2::ApiDiscoveredServerData &discoveredServer);

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection);
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection);

    virtual void handleRemotePeerFound(QnUuid data, Qn::PeerType peerType);
    virtual void handleRemotePeerLost(QnUuid data, Qn::PeerType peerType);

    virtual void onGotInitialNotification(const ec2::ApiFullInfoData& fullData);
    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        Qn::ResourceStatus status,
        ec2::NotificationSource source) = 0;
    virtual void execBusinessActionInternal(const QnAbstractBusinessActionPtr& /*action*/) {}

    void resetResourceTypes(const ec2::ApiResourceTypeDataList& resTypes);
    void resetResources(const ec2::ApiFullInfoData& fullData);
    void resetLicenses(const ec2::ApiLicenseDataList& licenses);
    void resetCamerasWithArchiveList(const ec2::ApiServerFootageDataList& cameraHistoryList);
    void resetTime();

    virtual bool canRemoveResource(const QnUuid& resourceId);
    virtual void removeResourceIgnored(const QnUuid& resourceId);

    virtual QnResourceFactory* getResourceFactory() const = 0;

public slots:
    void on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &rule);
    void on_licenseChanged(const QnLicensePtr &license);
    void on_licenseRemoved(const QnLicensePtr &license);

private slots:
    void on_gotInitialNotification(const ec2::ApiFullInfoData& fullData);
    void on_gotDiscoveryData(const ec2::ApiDiscoveryData &discoveryData, bool addInformation);

    void on_remotePeerFound(QnUuid data, Qn::PeerType peerType);
    void on_remotePeerLost(QnUuid data, Qn::PeerType peerType);

    void on_resourceStatusChanged(const QnUuid &resourceId, Qn::ResourceStatus status, ec2::NotificationSource source);
    void on_resourceParamChanged(const ec2::ApiResourceParamWithRefData& param );
    void on_resourceParamRemoved(const ec2::ApiResourceParamWithRefData& param );
    void on_resourceRemoved(const QnUuid& resourceId );
    void on_resourceStatusRemoved(const QnUuid& resourceId);

    void on_accessRightsChanged(const ec2::ApiAccessRightsData& accessRights);
    void on_userRoleChanged(const ec2::ApiUserRoleData& userRole);
    void on_userRoleRemoved(const QnUuid& userRoleId);

    void on_cameraUserAttributesChanged(const ec2::ApiCameraAttributesData& userAttributes);
    void on_cameraUserAttributesRemoved(const QnUuid& cameraId);
    void on_cameraHistoryChanged(const ec2::ApiServerFootageData &cameraHistory);

    void on_mediaServerUserAttributesChanged(const ec2::ApiMediaServerUserAttributesData& userAttributes);
    void on_mediaServerUserAttributesRemoved(const QnUuid& serverId);

    void on_businessEventRemoved(const QnUuid &id);
    void on_businessActionBroadcasted(const QnAbstractBusinessActionPtr &businessAction);
    void on_businessRuleReset(const ec2::ApiBusinessRuleDataList& rules);
    void on_broadcastBusinessAction(const QnAbstractBusinessActionPtr& action);
    void on_execBusinessAction( const QnAbstractBusinessActionPtr& action );
private:
    template <class Datatype> void updateResources(const std::vector<Datatype>& resList, QHash<QnUuid, QnResourcePtr>& remoteResources);
protected:
    ec2::AbstractECConnectionPtr m_connection;
};

#define qnCommonMessageProcessor commonModule()->messageProcessor()