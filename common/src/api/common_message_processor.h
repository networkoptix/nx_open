#pragma once

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <nx/vms/event/event_fwd.h>

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
    virtual void updateResource(const QnResourcePtr& resource, ec2::NotificationSource source);

    virtual void updateResource(const ec2::ApiUserData& user, ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::LayoutData& layout,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::VideowallData& videowall,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::WebPageData& webpage,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::CameraData& camera,
        ec2::NotificationSource source);
    virtual void updateResource(
        const ec2::ApiMediaServerData& server,
        ec2::NotificationSource source);
    virtual void updateResource(
        const ec2::ApiStorageData& storage,
        ec2::NotificationSource source);

    void resetServerUserAttributesList(
        const ec2::ApiMediaServerUserAttributesDataList& serverUserAttributesList);
    void resetCameraUserAttributesList(
        const nx::vms::api::CameraAttributesDataList& cameraUserAttributesList);
    void resetPropertyList(const nx::vms::api::ResourceParamWithRefDataList& params);
    void resetStatusList(const nx::vms::api::ResourceStatusDataList& params);
    void resetAccessRights(const nx::vms::api::AccessRightsDataList& accessRights);
    void resetUserRoles(const ec2::ApiUserRoleDataList& roles);
    void resetEventRules(const nx::vms::api::EventRuleDataList& eventRules);

signals:
    void connectionOpened();
    void connectionClosed();
    void connectionReset( QnCommonMessageProcessor* );

    void initialResourcesReceived();

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessActionReceived(const nx::vms::event::AbstractActionPtr& action);
    void execBusinessAction(const nx::vms::event::AbstractActionPtr& action);

    void videowallControlMessageReceived(const nx::vms::api::VideowallControlMessageData& message);

    void runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo);
    void remotePeerFound(QnUuid data, Qn::PeerType peerType);
    void remotePeerLost(QnUuid data, Qn::PeerType peerType);

    void syncTimeChanged(qint64 syncTime);

    void discoveredServerChanged(const ec2::ApiDiscoveredServerData &discoveredServer);

protected:
    virtual Qt::ConnectionType handlerConnectionType() const;

    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection);
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection);

    virtual void handleRemotePeerFound(QnUuid data, Qn::PeerType peerType);
    virtual void handleRemotePeerLost(QnUuid data, Qn::PeerType peerType);

    virtual void onGotInitialNotification(const ec2::ApiFullInfoData& fullData);
    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        Qn::ResourceStatus status,
        ec2::NotificationSource source) = 0;
    virtual void execBusinessActionInternal(const nx::vms::event::AbstractActionPtr& /*action*/) {}

    virtual void handleTourAddedOrUpdated(const nx::vms::api::LayoutTourData& tour);

    void resetResourceTypes(const nx::vms::api::ResourceTypeDataList& resTypes);
    void resetResources(const ec2::ApiFullInfoData& fullData);
    void resetLicenses(const ec2::ApiLicenseDataList& licenses);
    void resetCamerasWithArchiveList(const nx::vms::api::ServerFootageDataList& cameraHistoryList);
    void resetTime();

    virtual bool canRemoveResource(const QnUuid& resourceId);
    virtual void removeResourceIgnored(const QnUuid& resourceId);

    virtual QnResourceFactory* getResourceFactory() const = 0;

public slots:

    void on_licenseChanged(const QnLicensePtr &license);
    void on_licenseRemoved(const QnLicensePtr &license);

private slots:
    void on_gotInitialNotification(const ec2::ApiFullInfoData& fullData);
    void on_gotDiscoveryData(const ec2::ApiDiscoveryData &discoveryData, bool addInformation);

    void on_remotePeerFound(QnUuid data, Qn::PeerType peerType);
    void on_remotePeerLost(QnUuid data, Qn::PeerType peerType);

    void on_resourceStatusChanged(
        const QnUuid& resourceId,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source);
    void on_resourceParamChanged(const nx::vms::api::ResourceParamWithRefData& param );
    void on_resourceParamRemoved(const nx::vms::api::ResourceParamWithRefData& param );
    void on_resourceRemoved(const QnUuid& resourceId );
    void on_resourceStatusRemoved(const QnUuid& resourceId);

    void on_accessRightsChanged(const nx::vms::api::AccessRightsData& accessRights);
    void on_userRoleChanged(const ec2::ApiUserRoleData& userRole);
    void on_userRoleRemoved(const QnUuid& userRoleId);

    void on_cameraUserAttributesChanged(const nx::vms::api::CameraAttributesData& userAttributes);
    void on_cameraUserAttributesRemoved(const QnUuid& cameraId);
    void on_cameraHistoryChanged(const nx::vms::api::ServerFootageData& cameraHistory);

    void on_mediaServerUserAttributesChanged(const ec2::ApiMediaServerUserAttributesData& userAttributes);
    void on_mediaServerUserAttributesRemoved(const QnUuid& serverId);

    void on_businessEventRemoved(const QnUuid &id);
    void on_businessActionBroadcasted(const nx::vms::event::AbstractActionPtr& businessAction);
    void on_broadcastBusinessAction(const nx::vms::event::AbstractActionPtr& action);
    void on_execBusinessAction( const nx::vms::event::AbstractActionPtr& action );

    void on_eventRuleAddedOrUpdated(const nx::vms::api::EventRuleData& data);
protected:
    ec2::AbstractECConnectionPtr m_connection;
};

#define qnCommonMessageProcessor commonModule()->messageProcessor()
