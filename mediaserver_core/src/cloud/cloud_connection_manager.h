#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/cloud/cdb/api/connection.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/subscription.h>

#include <core/resource/resource_fwd.h>
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
    /**
     * @return null if not connected to the cloud.
     */
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection(
        const QString& cloudSystemId,
        const QString& cloudAuthKey) const;
    std::unique_ptr<nx::cdb::api::Connection> getCloudConnection();
    const nx::cdb::api::ConnectionFactory& connectionFactory() const;

    void processCloudErrorCode(nx::cdb::api::ResultCode resultCode);

    void setProxyVia(const SocketAddress& proxyEndpoint);

    bool detachSystemFromCloud();

    bool removeCloudUsers();
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

private slots:
    void cloudSettingsChanged();
};
