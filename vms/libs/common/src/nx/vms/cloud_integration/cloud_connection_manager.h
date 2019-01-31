#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/cloud/db/api/connection.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/retry_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/subscription.h>
#include <nx/utils/url.h>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace nx {
namespace vms {
namespace cloud_integration {

class AbstractCloudConnectionManager:
    public QObject,
    public nx::hpm::api::AbstractCloudSystemCredentialsProvider
{
    Q_OBJECT

public:
    virtual ~AbstractCloudConnectionManager() = default;

    virtual bool boundToCloud() const = 0;

    virtual std::unique_ptr<nx::cloud::db::api::Connection> getCloudConnection(
        const QString& cloudSystemId,
        const QString& cloudAuthKey) const = 0;

    virtual std::unique_ptr<nx::cloud::db::api::Connection> getCloudConnection() = 0;

    virtual void processCloudErrorCode(nx::cloud::db::api::ResultCode resultCode) = 0;

signals:
    void cloudBindingStatusChanged(bool boundToCloud);
};

class CloudConnectionManager:
    public AbstractCloudConnectionManager,
    public Qn::EnableSafeDirectConnection,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT

public:
    CloudConnectionManager(QnCommonModule* commonModule);
    ~CloudConnectionManager();

    virtual std::optional<nx::hpm::api::SystemCredentials>
        getSystemCredentials() const override;

    virtual bool boundToCloud() const override;
    /**
     * @return null if not connected to the cloud.
     */
    virtual std::unique_ptr<nx::cloud::db::api::Connection> getCloudConnection(
        const QString& cloudSystemId,
        const QString& cloudAuthKey) const override;

    virtual std::unique_ptr<nx::cloud::db::api::Connection> getCloudConnection() override;

    virtual void processCloudErrorCode(nx::cloud::db::api::ResultCode resultCode) override;

    void setCloudDbUrl(const nx::utils::Url &url);

    const nx::cloud::db::api::ConnectionFactory& connectionFactory() const;

    void setProxyVia(const nx::network::SocketAddress& proxyEndpoint);

    bool detachSystemFromCloud();

    bool removeCloudUsers();

signals:
    void connectedToCloud();
    void disconnectedFromCloud();

private:
    boost::optional<nx::network::SocketAddress> m_proxyAddress;
    mutable QnMutex m_mutex;
    std::unique_ptr<
        nx::cloud::db::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)> m_cdbConnectionFactory;

    void setCloudCredentials(const QString& cloudSystemId, const QString& cloudAuthKey);
    bool makeSystemLocal();

private slots:
    void cloudSettingsChanged();
};

} // namespace cloud_integration
} // namespace vms
} // namespace nx
