#ifndef COMMON_MESSAGE_PROCESSOR_H
#define COMMON_MESSAGE_PROCESSOR_H

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <business/business_fwd.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "nx_ec/data/api_runtime_data.h"

#include <utils/common/singleton.h>
#include <utils/common/connective.h>

class QnCommonMessageProcessor: public Connective<QObject>, public Singleton<QnCommonMessageProcessor>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit QnCommonMessageProcessor(QObject *parent = 0);
    virtual ~QnCommonMessageProcessor() {}

    virtual void init(const ec2::AbstractECConnectionPtr& connection);

    virtual void updateResource(const QnResourcePtr &resource);

    QMap<QnUuid, QnBusinessEventRulePtr> businessRules() const;

    void processServerUserAttributesList( const QnMediaServerUserAttributesList& serverUserAttributesList );
    void processCameraUserAttributesList( const QnCameraUserAttributesList& cameraUserAttributesList );
    void processPropertyList(const ec2::ApiResourceParamWithRefDataList& params);
    void processStatusList(const ec2::ApiResourceStatusDataList& params);
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

    void videowallControlMessageReceived(const QnVideoWallControlMessage &message);

    void cameraBookmarkTagsAdded(const QnCameraBookmarkTags &tags);
    void cameraBookmarkTagsRemoved(const QnCameraBookmarkTags &tags);

    void runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo);
    void remotePeerFound(const ec2::ApiPeerAliveData &data);
    void remotePeerLost(const ec2::ApiPeerAliveData &data);

    void syncTimeChanged(qint64 syncTime);
    void peerTimeChanged(const QnUuid &peerId, qint64 syncTime, qint64 peerTime);
    void timeServerSelectionRequired();
protected:
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData);
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) = 0;
    virtual void execBusinessActionInternal(const QnAbstractBusinessActionPtr& /*action*/) {}
    
    virtual void afterRemovingResource(const QnUuid &id);

    virtual void processResources(const QnResourceList &resources);
    void processLicenses(const QnLicenseList &licenses);
    void processCameraServerItems(const QnCameraHistoryList &cameraHistoryList);
    
    virtual bool canRemoveResource(const QnUuid& resourceId);
    virtual void removeResourceIgnored(const QnUuid& resourceId);

public slots:
    void on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &rule);
    void on_licenseChanged(const QnLicensePtr &license);
    void on_licenseRemoved(const QnLicensePtr &license);

private slots:
    void at_remotePeerFound(ec2::ApiPeerAliveData data);
    void at_remotePeerLost(ec2::ApiPeerAliveData data);

    void on_gotInitialNotification(const ec2::QnFullResourceData &fullData);
    void on_gotDiscoveryData(const ec2::ApiDiscoveryData &discoveryData, bool addInformation);

    void on_resourceStatusChanged(const QnUuid &resourceId, Qn::ResourceStatus status );
    void on_resourceParamChanged(const ec2::ApiResourceParamWithRefData& param );
    void on_resourceRemoved(const QnUuid& resourceId );

    void on_cameraUserAttributesChanged(const QnCameraUserAttributesPtr& userAttributes);
    void on_cameraUserAttributesRemoved(const QnUuid& cameraID);
    void on_cameraHistoryChanged(const QnCameraHistoryItemPtr &cameraHistory);
    void on_cameraHistoryRemoved(const QnCameraHistoryItemPtr &cameraHistory);

    void on_mediaServerUserAttributesChanged(const QnMediaServerUserAttributesPtr& userAttributes);
    void on_mediaServerUserAttributesRemoved(const QnUuid& serverID);

    void on_businessEventRemoved(const QnUuid &id);
    void on_businessActionBroadcasted(const QnAbstractBusinessActionPtr &businessAction);
    void on_businessRuleReset(const QnBusinessEventRuleList &rules);
    void on_broadcastBusinessAction(const QnAbstractBusinessActionPtr& action);
    void on_execBusinessAction( const QnAbstractBusinessActionPtr& action );
    void on_panicModeChanged(Qn::PanicMode mode);   
protected:
    ec2::AbstractECConnectionPtr m_connection;
    QMap<QnUuid, QnBusinessEventRulePtr> m_rules;
};

#endif // COMMON_MESSAGE_PROCESSOR_H
