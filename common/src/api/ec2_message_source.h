#ifndef _EC2_MESSAGE_SOURCE_H__
#define _EC2_MESSAGE_SOURCE_H__

#include <atomic>

#include "nx_ec/ec_api.h"
#include "app_server_connection.h"
#include "message.h"


class QnMessageSource2 : public QObject
{
    Q_OBJECT

public:
    QnMessageSource2(ec2::AbstractECConnectionPtr connection);

signals:
    void connectionOpened(QnMessage message);
    void messageReceived(QnMessage message);

private slots:
    void on_gotInitialNotification( ec2::QnFullResourceData fullData );
    void on_runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo );

    void on_resourceStatusChanged( const QnId& resourceId, QnResource::Status status );
    void on_resourceDisabledChanged( const QnId& resourceId, bool disabled );
    void on_resourceChanged( const QnResourcePtr& resource );
    void on_resourceParamsChanged( const QnId& resourceId, const QnKvPairList& kvPairs );
    void on_resourceRemoved( const QnId& resourceId );

    void on_mediaServerAddedOrUpdated( QnMediaServerResourcePtr camera );
    void on_mediaServerRemoved( QnId id );

    void on_cameraAddedOrUpdated( QnVirtualCameraResourcePtr camera );
    void on_cameraHistoryChanged( QnCameraHistoryItemPtr cameraHistory );
    void on_cameraRemoved( QnId id );

    void on_licenseChanged(QnLicensePtr license);

    void on_businessEventAddedOrUpdated( QnBusinessEventRulePtr camera );
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

private:
    std::atomic<int> m_seqNumber;
};


#endif // _EC2_MESSAGE_SOURCE_H__
