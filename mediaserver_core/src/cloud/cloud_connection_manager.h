/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CLOUD_CONNECTION_MANAGER_H
#define NX_MS_CLOUD_CONNECTION_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include <cdb/connection.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/safe_direct_connection.h>
#include <utils/common/subscription.h>


class CloudConnectionManager
:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider
{
    Q_OBJECT

public:
    CloudConnectionManager();
    ~CloudConnectionManager();

    virtual boost::optional<nx::hpm::api::SystemCredentials>
        getSystemCredentials() const override;

    bool boundToCloud() const;
    /** Returns \a nullptr if not connected to the cloud */
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection();
    const nx::cdb::api::ConnectionFactory& connectionFactory() const;

    void processCloudErrorCode(nx::cdb::api::ResultCode resultCode);

    //event subscription
    void subscribeToSystemAccessListUpdatedEvent(
        nx::utils::MoveOnlyFunc<void(nx::cdb::api::SystemAccessListModifiedEvent)> handler,
        nx::utils::SubscriptionId* const subscriptionId);
    void unsubscribeFromSystemAccessListUpdatedEvent(
        nx::utils::SubscriptionId subscriptionId);

signals:
    void cloudBindingStatusChanged(bool boundToCloud);

private:
    QString m_cloudSystemID;
    QString m_cloudAuthKey;
    mutable QnMutex m_mutex;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_cdbConnectionFactory;
    std::unique_ptr<nx::cdb::api::EventConnection> m_eventConnection;
    nx::utils::Subscription<nx::cdb::api::SystemAccessListModifiedEvent>
        m_systemAccessListUpdatedEventSubscription;
    std::unique_ptr<nx::network::RetryTimer> m_eventConnectionRetryTimer;

    bool boundToCloud(QnMutexLockerBase* const lk) const;
    void monitorForCloudEvents();
    void stopMonitoringCloudEvents();
    void onSystemAccessListUpdated(nx::cdb::api::SystemAccessListModifiedEvent);
    void startEventConnection();
    void onEventConnectionEstablished(nx::cdb::api::ResultCode resultCode);

private slots:
    void cloudSettingsChanged();
};

#endif  //NX_MS_CLOUD_CONNECTION_MANAGER_H
