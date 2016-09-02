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

CloudConnectionManager::CloudConnectionManager()
:
    m_cdbConnectionFactory(createConnectionFactory(), destroyConnectionFactory)
{
    const auto cdbEndpoint = MSSettings::roSettings()->value(
        nx_ms_conf::CDB_ENDPOINT,
        "").toString();
    if (!cdbEndpoint.isEmpty())
    {
        const auto hostAndPort = cdbEndpoint.split(":");
        if (hostAndPort.size() == 2)
        {
            m_cdbConnectionFactory->setCloudEndpoint(
                hostAndPort[0].toStdString(),
                hostAndPort[1].toInt());
        }
    }

    Qn::directConnect(
        qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged,
        this, &CloudConnectionManager::cloudSettingsChanged);
}

CloudConnectionManager::~CloudConnectionManager()
{
    directDisconnectAll();

    if (m_eventConnection)
        stopMonitoringCloudEvents();
}

void CloudConnectionManager::setProxyVia(const SocketAddress& proxyEndpoint)
{
    m_proxyAddress = proxyEndpoint;
}

boost::optional<nx::hpm::api::SystemCredentials>
    CloudConnectionManager::getSystemCredentials() const
{
    QnMutexLocker lk(&m_mutex);
    if (m_cloudSystemID.isEmpty() || m_cloudAuthKey.isEmpty())
        return boost::none;

    nx::hpm::api::SystemCredentials cloudCredentials;
    cloudCredentials.systemId = m_cloudSystemID.toUtf8();
    cloudCredentials.serverId = qnCommon->moduleGUID().toByteArray();
    cloudCredentials.key = m_cloudAuthKey.toUtf8();
    return cloudCredentials;
}

bool CloudConnectionManager::boundToCloud() const
{
    QnMutexLocker lk(&m_mutex);
    return boundToCloud(&lk);
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection(
    const QString& cloudSystemID,
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
    result->setCredentials(cloudSystemID.toStdString(), cloudAuthKey.toStdString());
    result->setProxyCredentials(proxyLogin.toStdString(), proxyPassword.toStdString());
    result->setProxyVia(m_proxyAddress);
    return result;
}

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection()
{
    QnMutexLocker lk(&m_mutex);
    if (!boundToCloud(&lk))
        return nullptr;
    return getCloudConnection(
        m_cloudSystemID,
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
        if (!cleanupCloudDataInLocalDB())
        {
            NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
        }
    }
}

void CloudConnectionManager::subscribeToSystemAccessListUpdatedEvent(
    nx::utils::MoveOnlyFunc<void(nx::cdb::api::SystemAccessListModifiedEvent)> handler,
    nx::utils::SubscriptionId* const subscriptionId)
{
    m_systemAccessListUpdatedEventSubscription.subscribe(
        std::move(handler),
        subscriptionId);
}

void CloudConnectionManager::unsubscribeFromSystemAccessListUpdatedEvent(
    nx::utils::SubscriptionId subscriptionId)
{
    m_systemAccessListUpdatedEventSubscription.removeSubscription(subscriptionId);
}

bool CloudConnectionManager::cleanupCloudDataInLocalDB()
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

    qnCommon->updateModuleInformation();
    MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey, "no");

    return true;
}

bool CloudConnectionManager::boundToCloud(QnMutexLockerBase* const /*lk*/) const
{
    return !m_cloudSystemID.isEmpty() && !m_cloudAuthKey.isEmpty();
}

void CloudConnectionManager::monitorForCloudEvents()
{
    QString proxyLogin;
    QString proxyPassword;

    auto server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (server)
    {
        proxyLogin = server->getId().toString();
        proxyPassword = server->getAuthKey();
    }

    m_eventConnection = m_cdbConnectionFactory->createEventConnection();
    m_eventConnection->setCredentials(m_cloudSystemID.toStdString(), m_cloudAuthKey.toStdString());
    m_eventConnection->setProxyCredentials(proxyLogin.toStdString(), proxyPassword.toStdString());
    m_eventConnection->setProxyVia(m_proxyAddress);

    m_eventConnectionRetryTimer = std::make_unique<nx::network::RetryTimer>(
        nx::network::RetryPolicy(
            nx::network::RetryPolicy::kInfiniteRetries,
            std::chrono::seconds(1),
            nx::network::RetryPolicy::kDefaultDelayMultiplier,
            kMaxEventConnectionStartRetryPeriod));
    m_eventConnectionRetryTimer->post(
        std::bind(&CloudConnectionManager::startEventConnection, this));
}

void CloudConnectionManager::stopMonitoringCloudEvents()
{
    if (!m_eventConnection)
        return;

    m_eventConnectionRetryTimer->pleaseStopSync();

    //closing event connection
    decltype(m_eventConnection) eventConnection;
    {
        QnMutexLocker lk(&m_mutex);
        NX_ASSERT(m_eventConnection);
        eventConnection = std::move(m_eventConnection);
        m_eventConnection = nullptr;
    }

    eventConnection.reset();
    m_eventConnectionRetryTimer.reset();
}

void CloudConnectionManager::onSystemAccessListUpdated(
    nx::cdb::api::SystemAccessListModifiedEvent event)
{
    m_systemAccessListUpdatedEventSubscription.notify(std::move(event));
}

void CloudConnectionManager::startEventConnection()
{
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_eventConnection)
            return;
    }

    using namespace std::placeholders;
    nx::cdb::api::SystemEventHandlers systemEventHandlers;
    systemEventHandlers.onSystemAccessListUpdated =
        std::bind(&CloudConnectionManager::onSystemAccessListUpdated, this, _1);
    m_eventConnection->start(
        std::move(systemEventHandlers),
        std::bind(&CloudConnectionManager::onEventConnectionEstablished, this, _1));
}

void CloudConnectionManager::onEventConnectionEstablished(
    nx::cdb::api::ResultCode resultCode)
{
    if (resultCode == nx::cdb::api::ResultCode::ok)
    {
        NX_LOGX(lm("Successfully opened event connection to the cloud"), cl_logDEBUG2);
        return;
    }

    NX_LOGX(lm("Error opening event connection to the cloud: %1")
        .arg(nx::cdb::api::toString(resultCode)), cl_logDEBUG1);

    if (!m_eventConnectionRetryTimer->scheduleNextTry(
            std::bind(&CloudConnectionManager::startEventConnection, this)))
    {
        NX_ASSERT(false);
    }
}

void CloudConnectionManager::cloudSettingsChanged()
{
    const auto cloudSystemId = qnGlobalSettings->cloudSystemID();
    const auto cloudAuthKey = qnGlobalSettings->cloudAuthKey();

    QnMutexLocker lk(&m_mutex);
    if (cloudSystemId == m_cloudSystemID &&
        cloudAuthKey == m_cloudAuthKey)
    {
        return;
    }

    m_cloudSystemID = cloudSystemId;
    m_cloudAuthKey = cloudAuthKey;
    const bool boundToCloud = !m_cloudSystemID.isEmpty() && !m_cloudAuthKey.isEmpty();

    lk.unlock();

    QnModuleInformation info = qnCommon->moduleInformation();
    if (info.cloudSystemId != cloudSystemId)
    {
        info.cloudSystemId = cloudSystemId;
        qnCommon->setModuleInformation(info);
    }

    if (boundToCloud)
    {
        nx::hpm::api::SystemCredentials credentials(
            cloudSystemId.toUtf8(),
            qnCommon->moduleGUID().toSimpleString().toUtf8(),
            cloudAuthKey.toUtf8());

        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(std::move(credentials));

        monitorForCloudEvents();
        MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey, "yes");
    }
    else
    {
        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(boost::none);
        stopMonitoringCloudEvents();
        MSSettings::roSettings()->setValue(QnServer::kIsConnectedToCloudKey, "no");
    }

    emit cloudBindingStatusChanged(boundToCloud);
    if (boundToCloud)
        emit connectedToCloud();
    else
        emit disconnectedFromCloud();
}
