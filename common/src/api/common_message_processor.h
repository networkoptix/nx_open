#ifndef COMMON_MESSAGE_PROCESSOR_H
#define COMMON_MESSAGE_PROCESSOR_H

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <business/business_fwd.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "nx_ec/data/api_runtime_data.h"

#include <nx/utils/singleton.h>
#include <utils/common/connective.h>

class QnResourceFactory;

class QnCommonMessageProcessor: public Connective<QObject>, public Singleton<QnCommonMessageProcessor>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit QnCommonMessageProcessor(QObject *parent = 0);
    virtual ~QnCommonMessageProcessor() {}

    virtual void init(const ec2::AbstractECConnectionPtr& connection);

    virtual void updateResource(const QnResourcePtr &resource);
    virtual void updateResource(const ec2::ApiUserData& user);
    virtual void updateResource(const ec2::ApiLayoutData& layout);
    virtual void updateResource(const ec2::ApiVideowallData& videowall);
    virtual void updateResource(const ec2::ApiWebPageData& webpage);
    virtual void updateResource(const ec2::ApiCameraData& camera);
    virtual void updateResource(const ec2::ApiMediaServerData& server);
    virtual void updateResource(const ec2::ApiStorageData& storage);

    QMap<QnUuid, QnBusinessEventRulePtr> businessRules() const;

    void resetServerUserAttributesList( const ec2::ApiMediaServerUserAttributesDataList& serverUserAttributesList );
    void resetCameraUserAttributesList( const ec2::ApiCameraAttributesDataList& cameraUserAttributesList );
    void resetPropertyList(const ec2::ApiResourceParamWithRefDataList& params);
    void resetStatusList(const ec2::ApiResourceStatusDataList& params);
signals:
    void connectionOpened();
    void connectionClosed();
    void connectionReset( QnCommonMessageProcessor* );

    void initialResourcesReceived();

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessRuleChanged(const QnBusinessEventRulePtr &rule);
    void businessRuleDeleted(const QnUuid &id);
    void businessRuleReset(const QnBusinessEventRuleList &rules);
    void businessActionReceived(const QnAbstractBusinessActionPtr& action);
    void execBusinessAction(const QnAbstractBusinessActionPtr& action);

    void videowallControlMessageReceived(const ec2::ApiVideowallControlMessageData& message);

    void runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo);
    void remotePeerFound(const ec2::ApiPeerAliveData &data);
    void remotePeerLost(const ec2::ApiPeerAliveData &data);

    void syncTimeChanged(qint64 syncTime);
    void peerTimeChanged(const QnUuid &peerId, qint64 syncTime, qint64 peerTime);
    void timeServerSelectionRequired();

    void discoveredServerChanged(const ec2::ApiDiscoveredServerData &discoveredServer);

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection);
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection);

    virtual void handleRemotePeerFound(const ec2::ApiPeerAliveData &data);
    virtual void handleRemotePeerLost(const ec2::ApiPeerAliveData &data);

    virtual void onGotInitialNotification(const ec2::ApiFullInfoData& fullData);
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) = 0;
    virtual void execBusinessActionInternal(const QnAbstractBusinessActionPtr& /*action*/) {}

    void resetResourceTypes(const ec2::ApiResourceTypeDataList& resTypes);
    void resetResources(const ec2::ApiFullInfoData& fullData);
    void resetLicenses(const ec2::ApiLicenseDataList& licenses);
    void resetCamerasWithArchiveList(const ec2::ApiServerFootageDataList &cameraHistoryList);
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

    void on_remotePeerFound(const ec2::ApiPeerAliveData& data);
    void on_remotePeerLost(const ec2::ApiPeerAliveData& data);

    void on_resourceStatusChanged(const QnUuid &resourceId, Qn::ResourceStatus status );
    void on_resourceParamChanged(const ec2::ApiResourceParamWithRefData& param );
    void on_resourceRemoved(const QnUuid& resourceId );

    void on_cameraUserAttributesChanged(const ec2::ApiCameraAttributesData& userAttributes);
    void on_cameraUserAttributesRemoved(const QnUuid& cameraID);
    void on_cameraHistoryChanged(const ec2::ApiServerFootageData &cameraHistory);

    void on_mediaServerUserAttributesChanged(const ec2::ApiMediaServerUserAttributesData& userAttributes);
    void on_mediaServerUserAttributesRemoved(const QnUuid& serverID);

    void on_businessEventRemoved(const QnUuid &id);
    void on_businessActionBroadcasted(const QnAbstractBusinessActionPtr &businessAction);
    void on_businessRuleReset(const ec2::ApiBusinessRuleDataList& rules);
    void on_broadcastBusinessAction(const QnAbstractBusinessActionPtr& action);
    void on_execBusinessAction( const QnAbstractBusinessActionPtr& action );
protected:
    ec2::AbstractECConnectionPtr m_connection;
    QMap<QnUuid, QnBusinessEventRulePtr> m_rules;
};

#endif // COMMON_MESSAGE_PROCESSOR_H
