// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/data_fwd.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>
#include <nx_ec/ec_api_fwd.h>

Q_MOC_INCLUDE(<nx/vms/api/data/discovery_data.h>)
Q_MOC_INCLUDE(<nx/vms/api/data/showreel_data.h>)
Q_MOC_INCLUDE(<nx/vms/api/rules/rule.h>)

class QnResourceFactory;

namespace nx::vms::api { struct AccessRightsDataDeprecated; }

class NX_VMS_COMMON_API QnCommonMessageProcessor:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnCommonMessageProcessor(
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~QnCommonMessageProcessor();

    virtual void init(const ec2::AbstractECConnectionPtr& connection);

    ec2::AbstractECConnectionPtr connection() const;
    nx::Uuid currentPeerId() const;

    /**
     * @param resource resource to update
     * @param peerId peer what modified resource.
     */
    virtual void updateResource(const QnResourcePtr& resource, ec2::NotificationSource source);

    virtual void updateResource(
        const nx::vms::api::UserData& user,
        ec2::NotificationSource source);
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
        const nx::vms::api::AnalyticsPluginData& analyticsPluginData,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::AnalyticsEngineData& analyticsEngineData,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::CameraData& camera,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::MediaServerData& server,
        ec2::NotificationSource source);
    virtual void updateResource(
        const nx::vms::api::StorageData& storage,
        ec2::NotificationSource source);

    void updateLicense(const QnLicensePtr& license);

    void resetServerUserAttributesList(
        const nx::vms::api::MediaServerUserAttributesDataList& serverUserAttributesList);
    void resetCameraUserAttributesList(
        const nx::vms::api::CameraAttributesDataList& cameraUserAttributesList);
    void resetPropertyList(const nx::vms::api::ResourceParamWithRefDataList& params);
    void resetStatusList(const nx::vms::api::ResourceStatusDataList& params);
    void resetUserGroups(const nx::vms::api::UserGroupDataList& userGroups);
    void resetEventRules(const nx::vms::api::EventRuleDataList& eventRules);
    void resetVmsRules(const nx::vms::api::rules::RuleList& vmsRules);
    virtual void addHardwareIdMapping(const nx::vms::api::HardwareIdMapping& /*data*/) {}
    virtual void removeHardwareIdMapping(const nx::Uuid& /*id*/) {}

signals:
    void connectionOpened();
    void connectionClosed();

    void initialResourcesReceived();

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessActionReceived(const QSharedPointer<nx::vms::event::AbstractAction>& action);
    void videowallControlMessageReceived(const nx::vms::api::VideowallControlMessageData& message);

    void vmsEventReceived(const nx::vms::api::rules::EventInfo& eventInfo);
    void vmsActionReceived(const nx::vms::api::rules::ActionInfo& eventInfo);

    void vmsRulesReset(nx::Uuid peerId, const nx::vms::api::rules::RuleList& vmsRules);
    void vmsRuleUpdated(const nx::vms::api::rules::Rule& rule, ec2::NotificationSource source);
    void vmsRuleRemoved(nx::Uuid id);

    void serverRuntimeEventOccurred(const nx::vms::api::ServerRuntimeEventData& eventData);
    void runtimeInfoChanged(const nx::vms::api::RuntimeData& runtimeInfo);
    void runtimeInfoRemoved(const nx::vms::api::IdData& data);

    void remotePeerFound(nx::Uuid data, nx::vms::api::PeerType peerType);
    void remotePeerLost(nx::Uuid data, nx::vms::api::PeerType peerType);

    void gotInitialDiscoveredServers(const nx::vms::api::DiscoveredServerDataList& discoveredServers);
    void discoveredServerChanged(const nx::vms::api::DiscoveredServerData& discoveredServer);

protected:
    virtual Qt::ConnectionType handlerConnectionType() const;

    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection);
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection);

    virtual void handleRemotePeerFound(nx::Uuid data, nx::vms::api::PeerType peerType);
    virtual void handleRemotePeerLost(nx::Uuid data, nx::vms::api::PeerType peerType);

    virtual void onGotInitialNotification(const nx::vms::api::FullInfoData& fullData);
    virtual void onResourceStatusChanged(
        const QnResourcePtr &resource,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source) = 0;

    virtual void handleTourAddedOrUpdated(const nx::vms::api::ShowreelData& tour);

    void resetResourceTypes(const nx::vms::api::ResourceTypeDataList& resTypes);
    void resetResources(const nx::vms::api::FullInfoData& fullData);
    void resetLicenses(const nx::vms::api::LicenseDataList& licenses);
    void resetCamerasWithArchiveList(const nx::vms::api::ServerFootageDataList& cameraHistoryList);

    virtual bool canRemoveResource(const nx::Uuid& resourceId, ec2::NotificationSource source);
    virtual void removeResourceIgnored(const nx::Uuid& resourceId);

    virtual bool canRemoveResourceProperty(const nx::Uuid& resourceId, const QString& propertyName);
    virtual void refreshIgnoredResourceProperty(
        const nx::Uuid& resourceId, const QString& propertyName);

    virtual QnResourceFactory* getResourceFactory() const = 0;

    virtual bool handleRemoteAnalyticsNotification(
        const nx::vms::api::ResourceParamWithRefData& param,
        ec2::NotificationSource source);

private:
    void on_initNotification(const nx::vms::api::FullInfoData& fullData);
    void on_gotDiscoveryData(const nx::vms::api::DiscoveryData& discoveryData, bool addInformation);

    void on_remotePeerFound(nx::Uuid data, nx::vms::api::PeerType peerType);
    void on_remotePeerLost(nx::Uuid data, nx::vms::api::PeerType peerType);

    void on_resourceStatusChanged(
        const nx::Uuid& resourceId,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source);
    void on_resourceParamChanged(
        const nx::vms::api::ResourceParamWithRefData& param,
        ec2::NotificationSource source);
    void on_resourceParamRemoved(const nx::vms::api::ResourceParamWithRefData& param);
    void on_resourceRemoved(const nx::Uuid& resourceId, ec2::NotificationSource source);
    void on_resourceStatusRemoved(const nx::Uuid& resourceId, ec2::NotificationSource source);

    void on_accessRightsChanged(const nx::vms::api::AccessRightsDataDeprecated& accessRights);
    void on_userGroupChanged(const nx::vms::api::UserGroupData& userGroup);
    void on_userGroupRemoved(const nx::Uuid& userGroupId);

    void on_cameraUserAttributesChanged(const nx::vms::api::CameraAttributesData& userAttributes);
    void on_cameraUserAttributesRemoved(const nx::Uuid& cameraId);
    void on_cameraHistoryChanged(const nx::vms::api::ServerFootageData& cameraHistory);

    void on_mediaServerUserAttributesChanged(
        const nx::vms::api::MediaServerUserAttributesData& userAttributes);
    void on_mediaServerUserAttributesRemoved(const nx::Uuid& serverId);

    void on_licenseRemoved(const QnLicensePtr &license);

    void on_businessEventRemoved(const nx::Uuid &id);
    void on_broadcastBusinessAction(const nx::vms::event::AbstractActionPtr& action);

    void on_eventRuleAddedOrUpdated(const nx::vms::api::EventRuleData& data);

protected:
    ec2::AbstractECConnectionPtr m_connection;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
