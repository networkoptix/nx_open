#include "cloud_connection_manager.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/socket_global.h>
#include <nx/vms/utils/vms_utils.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>

namespace {

constexpr const auto kMaxEventConnectionStartRetryPeriod = std::chrono::minutes(1);

} // namespace

namespace nx {
namespace vms {
namespace cloud_integration {

CloudConnectionManager::CloudConnectionManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_cdbConnectionFactory(createConnectionFactory(), destroyConnectionFactory)
{
    Qn::directConnect(
        globalSettings(), &QnGlobalSettings::initialized,
        this, &CloudConnectionManager::cloudSettingsChanged);

    Qn::directConnect(
        globalSettings(), &QnGlobalSettings::cloudCredentialsChanged,
        this, &CloudConnectionManager::cloudSettingsChanged);
}

void CloudConnectionManager::setCloudDbUrl(const QUrl& url)
{
    m_cdbConnectionFactory->setCloudUrl(url.toString().toStdString());
}

CloudConnectionManager::~CloudConnectionManager()
{
    directDisconnectAll();
}

boost::optional<nx::hpm::api::SystemCredentials>
    CloudConnectionManager::getSystemCredentials() const
{
    const auto cloudSystemId = qnGlobalSettings->cloudSystemId();
    const auto cloudAuthKey = qnGlobalSettings->cloudAuthKey();

    if (cloudSystemId.isEmpty() || cloudAuthKey.isEmpty())
        return boost::none;

    nx::hpm::api::SystemCredentials cloudCredentials;
    cloudCredentials.systemId = cloudSystemId.toUtf8();
    cloudCredentials.serverId = commonModule()->moduleGUID().toByteArray();
    cloudCredentials.key = cloudAuthKey.toUtf8();
    return cloudCredentials;
}

bool CloudConnectionManager::boundToCloud() const
{
    const auto cloudSystemId = qnGlobalSettings->cloudSystemId();
    const auto cloudAuthKey = qnGlobalSettings->cloudAuthKey();
    return !cloudSystemId.isEmpty() && !cloudAuthKey.isEmpty();
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection(
    const QString& cloudSystemId,
    const QString& cloudAuthKey) const
{
    QString proxyLogin;
    QString proxyPassword;

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(
        commonModule()->moduleGUID());
    if (server)
    {
        proxyLogin = server->getId().toString();
        proxyPassword = server->getAuthKey();
    }

    auto result = m_cdbConnectionFactory->createConnection();
    result->setCredentials(cloudSystemId.toStdString(), cloudAuthKey.toStdString());

    QnMutexLocker lock(&m_mutex);
    if (m_proxyAddress)
    {
        result->setProxyCredentials(
            proxyLogin.toStdString(),
            proxyPassword.toStdString());
        result->setProxyVia(
            m_proxyAddress->address.toString().toStdString(),
            m_proxyAddress->port);
    }
    return result;
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection()
{
    const auto cloudSystemId = qnGlobalSettings->cloudSystemId();
    const auto cloudAuthKey = qnGlobalSettings->cloudAuthKey();
    if (cloudSystemId.isEmpty() || cloudAuthKey.isEmpty())
        return nullptr;

    return getCloudConnection(
        cloudSystemId,
        cloudAuthKey);
}

const nx::cdb::api::ConnectionFactory& CloudConnectionManager::connectionFactory() const
{
    return *m_cdbConnectionFactory;
}

void CloudConnectionManager::processCloudErrorCode(
    nx::cdb::api::ResultCode resultCode)
{
    NX_LOGX(lm("Error %1 while referring to cloud")
        .arg(nx::cdb::api::toString(resultCode)), cl_logDEBUG1);

    if (resultCode == nx::cdb::api::ResultCode::credentialsRemovedPermanently)
    {
        NX_LOGX(lm("Error. Cloud reported %1 error. Removing local cloud credentials...")
            .arg(nx::cdb::api::toString(resultCode)), cl_logDEBUG1);

        // System has been disconnected from cloud: cleaning up cloud credentials...
        if (!detachSystemFromCloud())
        {
            NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
        }
    }
}

void CloudConnectionManager::setProxyVia(const SocketAddress& proxyEndpoint)
{
    QnMutexLocker lock(&m_mutex);

    NX_ASSERT(proxyEndpoint.port > 0);
    m_proxyAddress = proxyEndpoint;
}

bool CloudConnectionManager::detachSystemFromCloud()
{
    NX_DEBUG(this, lm("Detaching system %1 from cloud ")
        .arg(qnGlobalSettings->cloudSystemId()));

    if (!removeCloudUsers())
        return false;

    qnGlobalSettings->resetCloudParams();
    if (!qnGlobalSettings->synchronizeNowSync())
    {
        NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
        return false;
    }

    emit cloudBindingStatusChanged(false);
    emit disconnectedFromCloud();

    return true;
}

void CloudConnectionManager::setCloudCredentials(
    const QString& cloudSystemId,
    const QString& cloudAuthKey)
{
    NX_LOGX(lm("New cloud credentials: %1:%2")
        .arg(cloudSystemId).arg(cloudAuthKey.size()), cl_logINFO);

    if (cloudSystemId.isEmpty() != cloudAuthKey.isEmpty())
        return; //< Ignoring intermediate state.

    const bool boundToCloud = !cloudSystemId.isEmpty() && !cloudAuthKey.isEmpty();

    if (boundToCloud)
    {
        nx::hpm::api::SystemCredentials credentials(
            cloudSystemId.toUtf8(),
            commonModule()->moduleGUID().toSimpleString().toUtf8(),
            cloudAuthKey.toUtf8());

        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(std::move(credentials));
    }
    else
    {
        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(boost::none);
        makeSystemLocal();
    }

    emit cloudBindingStatusChanged(boundToCloud);
    if (boundToCloud)
        emit connectedToCloud();
    else
        emit disconnectedFromCloud();
}

bool CloudConnectionManager::makeSystemLocal()
{
    NX_LOGX(lm("Making system local"), cl_logINFO);

    auto adminUser = resourcePool()->getAdministrator();
    if (adminUser && !adminUser->isEnabled() && !qnGlobalSettings->localSystemId().isNull())
    {
        if (!nx::vms::utils::resetSystemToStateNew(commonModule()))
        {
            NX_LOGX(lit("Error resetting system state to new"), cl_logWARNING);
            return false;
        }
    }

    return removeCloudUsers();
}

bool CloudConnectionManager::removeCloudUsers()
{
    auto usersToRemove = resourcePool()->getResources<QnUserResource>().filtered(
        [](const QnUserResourcePtr& user)
        {
            return user->isCloud();
        });
    for (const auto& user: usersToRemove)
    {
        const auto errCode = commonModule()->ec2Connection()
            ->getUserManager(Qn::kSystemAccess)->removeSync(user->getId());
        NX_ASSERT(errCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
        if (errCode != ec2::ErrorCode::ok)
        {
            NX_LOGX(lit("Error removing cloud user (%1:%2) from local DB: %3")
                .arg(user->getId().toString()).arg(user->getName()).arg(ec2::toString(errCode)),
                cl_logWARNING);
            return false;
        }
    }

    return true;
}

void CloudConnectionManager::cloudSettingsChanged()
{
    NX_LOGX(lm("Cloud settings has been changed. cloudSystemId %1, cloudAuthKey %2")
        .arg(qnGlobalSettings->cloudSystemId()).arg(qnGlobalSettings->cloudAuthKey().size()),
        cl_logINFO);

    setCloudCredentials(
        qnGlobalSettings->cloudSystemId(),
        qnGlobalSettings->cloudAuthKey());
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
