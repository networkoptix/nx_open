/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cloud_connection_manager.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/socket_global.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

#include "media_server/settings.h"
#include "media_server/serverutil.h"
#include <core/resource/media_server_resource.h>
#include <server/server_globals.h>

namespace {
constexpr const auto kMaxEventConnectionStartRetryPeriod = std::chrono::minutes(1);
}

CloudConnectionManager::CloudConnectionManager():
    m_cdbConnectionFactory(createConnectionFactory(), destroyConnectionFactory)
{
    const auto cdbEndpoint = MSSettings::roSettings()->value(
        nx_ms_conf::CDB_ENDPOINT,
        "").toString();
    if (!cdbEndpoint.isEmpty())
        m_cdbConnectionFactory->setCloudUrl(lm("http://%1").arg(cdbEndpoint).toStdString());

    Qn::directConnect(
        qnGlobalSettings, &QnGlobalSettings::initialized,
        this, &CloudConnectionManager::cloudSettingsChanged);

    Qn::directConnect(
        qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
        this, &CloudConnectionManager::cloudSettingsChanged);
}

CloudConnectionManager::~CloudConnectionManager()
{
    directDisconnectAll();
}

void CloudConnectionManager::setProxyVia(const SocketAddress& proxyEndpoint)
{
    m_proxyAddress = proxyEndpoint;
}

boost::optional<nx::hpm::api::SystemCredentials>
    CloudConnectionManager::getSystemCredentials() const
{
    QnMutexLocker lk(&m_mutex);
    if (m_cloudSystemId.isEmpty() || m_cloudAuthKey.isEmpty())
        return boost::none;

    nx::hpm::api::SystemCredentials cloudCredentials;
    cloudCredentials.systemId = m_cloudSystemId.toUtf8();
    cloudCredentials.serverId = qnCommon->moduleGUID().toByteArray();
    cloudCredentials.key = m_cloudAuthKey.toUtf8();
    return cloudCredentials;
}

void CloudConnectionManager::setCloudCredentials(
    const QString& cloudSystemId,
    const QString& cloudAuthKey)
{
    QnMutexLocker lock(&m_mutex);

    if (cloudSystemId == m_cloudSystemId &&
        cloudAuthKey == m_cloudAuthKey)
    {
        return;
    }

    m_cloudSystemId = cloudSystemId;
    m_cloudAuthKey = cloudAuthKey;
    const bool boundToCloud = !m_cloudSystemId.isEmpty() && !m_cloudAuthKey.isEmpty();
    if (!boundToCloud)
    {
        m_cloudSystemId.clear();
        m_cloudAuthKey.clear();
    }

    lock.unlock();

    if (boundToCloud)
    {
        nx::hpm::api::SystemCredentials credentials(
            cloudSystemId.toUtf8(),
            qnCommon->moduleGUID().toSimpleString().toUtf8(),
            cloudAuthKey.toUtf8());

        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(std::move(credentials));

        MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey, "yes");
    }
    else
    {
        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(boost::none);
        MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey, "no");
    }

    if (!boundToCloud)
        makeSystemLocal();

    emit cloudBindingStatusChanged(boundToCloud);
    if (boundToCloud)
        emit connectedToCloud();
    else
        emit disconnectedFromCloud();
}

bool CloudConnectionManager::boundToCloud() const
{
    QnMutexLocker lk(&m_mutex);
    return boundToCloud(&lk);
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection(
    const QString& cloudSystemId,
    const QString& cloudAuthKey) const
{
    QString proxyLogin;
    QString proxyPassword;

    auto server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (server)
    {
        proxyLogin = server->getId().toString();
        proxyPassword = server->getAuthKey();
    }

    auto result = m_cdbConnectionFactory->createConnection();
    result->setCredentials(cloudSystemId.toStdString(), cloudAuthKey.toStdString());
    result->setProxyCredentials(proxyLogin.toStdString(), proxyPassword.toStdString());
    result->setProxyVia(m_proxyAddress.address.toString().toStdString(), m_proxyAddress.port);
    return result;
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection()
{
    QnMutexLocker lk(&m_mutex);
    if (!boundToCloud(&lk))
        return nullptr;
    return getCloudConnection(
        m_cloudSystemId,
        m_cloudAuthKey);
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

        //system has been disconnected from cloud: cleaning up cloud credentials...
        if (!detachSystemFromCloud())
        {
            NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
        }
    }
}

bool CloudConnectionManager::detachSystemFromCloud()
{
    qnGlobalSettings->resetCloudParams();
    return qnGlobalSettings->synchronizeNowSync();
}

bool CloudConnectionManager::resetCloudData()
{
    qnGlobalSettings->resetCloudParams();

    if (!qnGlobalSettings->synchronizeNowSync())
    {
        NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
        return false;
    }

    // removing cloud users
    auto usersToRemove = qnResPool->getResources<QnUserResource>().filtered(
        [](const QnUserResourcePtr& user)
        {
            return user->isCloud();
        });
    for (const auto& user: usersToRemove)
    {
        auto errCode = QnAppServerConnectionFactory::getConnection2()
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

    MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey, "no");

    return true;
}

bool CloudConnectionManager::makeSystemLocal()
{
    auto adminUser = qnResPool->getAdministrator();
    if (adminUser && !adminUser->isEnabled() && !qnGlobalSettings->localSystemId().isNull())
    {
        if (!resetSystemToStateNew())
        {
            NX_LOGX(lit("Error resetting system state to new"), cl_logWARNING);
            return false;
        }
    }

    return resetCloudData();
}

bool CloudConnectionManager::boundToCloud(QnMutexLockerBase* const /*lk*/) const
{
    return !m_cloudSystemId.isEmpty() && !m_cloudAuthKey.isEmpty();
}

void CloudConnectionManager::cloudSettingsChanged()
{
    setCloudCredentials(
        qnGlobalSettings->cloudSystemId(),
        qnGlobalSettings->cloudAuthKey());
}
