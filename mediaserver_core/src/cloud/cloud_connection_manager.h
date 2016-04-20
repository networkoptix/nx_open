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
#include <nx/utils/thread/mutex.h>
#include <nx/utils/singleton.h>
#include <utils/common/safe_direct_connection.h>


class CloudConnectionManager
:
    public QObject,
    public Singleton<CloudConnectionManager>,
    public Qn::EnableSafeDirectConnection,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider
{
    Q_OBJECT

public:
    CloudConnectionManager();
    ~CloudConnectionManager();

    virtual boost::optional<nx::hpm::api::SystemCredentials>
        getSystemCredentials() const override;

    bool bindedToCloud() const;
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection();
    const nx::cdb::api::ConnectionFactory& connectionFactory() const;

    void processCloudErrorCode(nx::cdb::api::ResultCode resultCode);

signals:
    void cloudBindingStatusChanged(bool bindedToCloud);

private:
    QString m_cloudSystemID;
    QString m_cloudAuthKey;
    mutable QnMutex m_mutex;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_cdbConnectionFactory;

    bool bindedToCloud(QnMutexLockerBase* const lk) const;
    void monitorForCloudEvents();
    void stopMonitoringCloudEvents();
    void onSystemAccessListUpdated(nx::cdb::api::SystemAccessListModifiedEvent);

private slots:
    void cloudSettingsChanged();
};

#endif  //NX_MS_CLOUD_CONNECTION_MANAGER_H
