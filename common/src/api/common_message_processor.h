#ifndef COMMON_MESSAGE_PROCESSOR_H
#define COMMON_MESSAGE_PROCESSOR_H

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <business/business_fwd.h>

#include <utils/common/singleton.h>
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_server_alive_data.h"
#include "nx_ec/data/api_runtime_data.h"

class QnCommonMessageProcessor: public QObject, public Singleton<QnCommonMessageProcessor>
{
    Q_OBJECT
public:
    explicit QnCommonMessageProcessor(QObject *parent = 0);
    virtual ~QnCommonMessageProcessor() {}

    virtual void init(const ec2::AbstractECConnectionPtr& connection);

    virtual void updateResource(const QnResourcePtr &resource) = 0;

signals:
    void connectionOpened();
    void connectionClosed();
    void connectionReset( QnCommonMessageProcessor* );

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessRuleChanged(const QnBusinessEventRulePtr &rule);
    void businessRuleDeleted(const QnId &id);
    void businessRuleReset(const QnBusinessEventRuleList &rules);
    void businessActionReceived(const QnAbstractBusinessActionPtr& action);
    void execBusinessAction(const QnAbstractBusinessActionPtr& action);

    void videowallControlMessageReceived(const QnVideoWallControlMessage &message);

    void cameraBookmarkTagsAdded(const QnCameraBookmarkTags &tags);
    void cameraBookmarkTagsRemoved(const QnCameraBookmarkTags &tags);

    void runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo);
    void remotePeerFound(const ec2::ApiPeerAliveData &data, bool isProxy);
    void remotePeerLost(const ec2::ApiPeerAliveData &data, bool isProxy);

protected:
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData);
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) = 0;
    virtual void execBusinessActionInternal(const QnAbstractBusinessActionPtr& /*action*/) {}
    
    virtual void afterRemovingResource(const QnId &id);

    virtual void processResources(const QnResourceList &resources);
    void processLicenses(const QnLicenseList &licenses);
    void processCameraServerItems(const QnCameraHistoryList &cameraHistoryList);
    
    virtual bool canRemoveResource(const QnId& resourceId);
    virtual void removeResourceIgnored(const QnId& resourceId);

public slots:
    void on_businessEventAddedOrUpdated(const QnBusinessEventRulePtr &rule);
    void on_licenseChanged(const QnLicensePtr &license);
    void on_licenseRemoved(const QnLicensePtr &license);

private slots:
    void on_gotInitialNotification(const ec2::QnFullResourceData &fullData);
    void on_gotDiscoveryData(const ec2::ApiDiscoveryDataList &discoveryData, bool addInformation);

    void on_resourceStatusChanged(const QnId &resourceId, QnResource::Status status );
    void on_resourceParamsChanged(const QnId& resourceId, const QnKvPairList& kvPairs );
    void on_resourceRemoved(const QnId& resourceId );

    void on_cameraHistoryChanged(const QnCameraHistoryItemPtr &cameraHistory);

    void on_businessEventRemoved(const QnId &id);
    void on_businessActionBroadcasted(const QnAbstractBusinessActionPtr &businessAction);
    void on_businessRuleReset(const QnBusinessEventRuleList &rules);
    void on_broadcastBusinessAction(const QnAbstractBusinessActionPtr& action);
    void on_execBusinessAction( const QnAbstractBusinessActionPtr& action );

    void on_panicModeChanged(Qn::PanicMode mode);   
protected:
    ec2::AbstractECConnectionPtr m_connection;
    QMap<QnId, QnBusinessEventRulePtr> m_rules;
};

#endif // COMMON_MESSAGE_PROCESSOR_H
