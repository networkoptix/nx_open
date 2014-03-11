#ifndef COMMON_MESSAGE_PROCESSOR_H
#define COMMON_MESSAGE_PROCESSOR_H

#include <QtCore/QObject>

#include <QtCore/QSharedPointer>

#include <api/model/kvpair.h>

#include <business/business_fwd.h>

#include <utils/common/singleton.h>
#include "nx_ec/ec_api.h"

class QnCommonMessageProcessor: public QObject, public Singleton<QnCommonMessageProcessor>
{
    Q_OBJECT
public:
    explicit QnCommonMessageProcessor(QObject *parent = 0);
    virtual ~QnCommonMessageProcessor() {}

    virtual void init(ec2::AbstractECConnectionPtr connection);

    virtual void updateResource(QnResourcePtr resource) = 0;

signals:
    void connectionOpened();
    void connectionClosed();
    void connectionReset( QnCommonMessageProcessor* );

    void fileAdded(const QString &filename);
    void fileUpdated(const QString &filename);
    void fileRemoved(const QString &filename);

    void businessRuleChanged(const QnBusinessEventRulePtr &rule);
    void businessRuleDeleted(QnId id);
    void businessRuleReset(QnBusinessEventRuleList rules);
    void businessActionReceived(const QnAbstractBusinessActionPtr& action);
protected:
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) = 0;
    virtual void onResourceStatusChanged(QnResourcePtr resource, QnResource::Status status) = 0;
    
    virtual void afterRemovingResource(const QnId& id);
public slots:
    void on_businessEventAddedOrUpdated( QnBusinessEventRulePtr camera );
private slots:
    void on_gotInitialNotification( ec2::QnFullResourceData fullData );
    void on_runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo );

    void on_resourceStatusChanged( const QnId& resourceId, QnResource::Status status );
    void on_resourceDisabledChanged( const QnId& resourceId, bool disabled );
    void on_resourceChanged( QnResourcePtr resource );
    void on_resourceParamsChanged( const QnId& resourceId, const QnKvPairList& kvPairs );
    void on_resourceRemoved( const QnId& resourceId );

    void on_mediaServerAddedOrUpdated( QnMediaServerResourcePtr camera );
    void on_mediaServerRemoved( QnId id );

    void on_cameraAddedOrUpdated( QnVirtualCameraResourcePtr camera );
    void on_cameraHistoryChanged( QnCameraHistoryItemPtr cameraHistory );
    void on_cameraRemoved( QnId id );

    void on_licenseChanged(QnLicensePtr license);

    void on_businessEventRemoved( QnId id );
    void on_businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
    void on_businessRuleReset( const QnBusinessEventRuleList& rules );
    void on_broadcastBusinessAction( const QnAbstractBusinessActionPtr& action );

    void on_userAddedOrUpdated( QnUserResourcePtr camera );
    void on_userRemoved( QnId id );

    void on_layoutAddedOrUpdated( QnLayoutResourcePtr camera );
    void on_layoutRemoved( QnId id );

    void on_storedFileAdded( QString filename );
    void on_storedFileUpdated( QString filename );
    void on_storedFileRemoved( QString filename );
    void on_panicModeChanged(Qn::PanicMode mode);
protected:
    ec2::AbstractECConnectionPtr m_connection;
    QMap<QnId, QnBusinessEventRulePtr> m_rules;
};

#endif // COMMON_MESSAGE_PROCESSOR_H
