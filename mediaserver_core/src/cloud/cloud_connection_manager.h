/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MS_CLOUD_CONNECTION_MANAGER_H
#define NX_MS_CLOUD_CONNECTION_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/cloud/cdb/api/connection.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/subscription.h>
#include <common/common_module_aware.h>

class CloudConnectionManager:
    public QObject,
    public Qn::EnableSafeDirectConnection,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    CloudConnectionManager(QnCommonModule* commonModule);
    ~CloudConnectionManager();

    virtual boost::optional<nx::hpm::api::SystemCredentials>
        getSystemCredentials() const override;

    bool boundToCloud() const;
    /** Returns \a nullptr if not connected to the cloud */
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection(
        const QString& cloudSystemId,
        const QString& cloudAuthKey) const;
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection();
    const nx::cdb::api::ConnectionFactory& connectionFactory() const;

    void processCloudErrorCode(nx::cdb::api::ResultCode resultCode);

    void setProxyVia(const SocketAddress& proxyEndpoint);

    bool detachSystemFromCloud();

signals:
    void cloudBindingStatusChanged(bool boundToCloud);
    void connectedToCloud();
    void disconnectedFromCloud();

private:
    boost::optional<SocketAddress> m_proxyAddress;
    mutable QnMutex m_mutex;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_cdbConnectionFactory;

    void setCloudCredentials(const QString& cloudSystemId, const QString& cloudAuthKey);
    bool makeSystemLocal();
    bool removeCloudUsers();
    void startEventConnection();
    void onEventConnectionEstablished(nx::cdb::api::ResultCode resultCode);

private slots:
    void cloudSettingsChanged();
};

#endif  //NX_MS_CLOUD_CONNECTION_MANAGER_H
