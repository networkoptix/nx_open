/**********************************************************
* Oct 2, 2015
* akolesnikov
***********************************************************/

#include "cloud_connection_manager.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <nx/fusion/serialization/lexical.h>

#include <nx/network/socket_global.h>

#include "media_server/settings.h"


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

std::unique_ptr<nx::cdb::api::Connection> CloudConnectionManager::getCloudConnection()
{
    QnMutexLocker lk(&m_mutex);
    if (!boundToCloud(&lk))
        return nullptr;
    return m_cdbConnectionFactory->createConnection(
        m_cloudSystemID.toStdString(),
        m_cloudAuthKey.toStdString());
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
        qnGlobalSettings->resetCloudParams();
        if (!qnGlobalSettings->synchronizeNowSync())
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

bool CloudConnectionManager::boundToCloud(QnMutexLockerBase* const /*lk*/) const
{
    return !m_cloudSystemID.isEmpty() && !m_cloudAuthKey.isEmpty();
}

void CloudConnectionManager::monitorForCloudEvents()
{
    m_eventConnection =
        m_cdbConnectionFactory->createEventConnection(
            m_cloudSystemID.toStdString(),
            m_cloudAuthKey.toStdString());

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
    m_eventConnectionRetryTimer->pleaseStopSync();
    m_eventConnectionRetryTimer.reset();

    //closing event connection
    NX_ASSERT(m_eventConnection);
    m_eventConnection.reset();
}

void CloudConnectionManager::onSystemAccessListUpdated(
    nx::cdb::api::SystemAccessListModifiedEvent event)
{
    m_systemAccessListUpdatedEventSubscription.notify(std::move(event));
}

void CloudConnectionManager::startEventConnection()
{
    NX_ASSERT(m_eventConnection);

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
    }
    else
    {
        nx::network::SocketGlobals::mediatorConnector()
            .setSystemCredentials(boost::none);
        stopMonitoringCloudEvents();
    }

    emit cloudBindingStatusChanged(boundToCloud);
}
