
#include "cloud_system_name_updater.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>

#include "cloud_connection_manager.h"


namespace {
constexpr const auto kSystemNameUpdatePeriod = std::chrono::hours(1);
constexpr const auto kRetrySystemNameUpdatePeriod = std::chrono::minutes(1);
}

using nx::utils::TimerManager;

CloudSystemNameUpdater::CloudSystemNameUpdater(
    CloudConnectionManager* const cloudConnectionManager)
:
    m_cloudConnectionManager(cloudConnectionManager),
    m_terminated(false),
    m_timerId(0)
{
    scheduleNextUpdate(std::chrono::seconds::zero());
}

CloudSystemNameUpdater::~CloudSystemNameUpdater()
{
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
    }

    //m_timerId cannot be modified

    if (m_timerId)
        TimerManager::instance()->joinAndDeleteTimer(m_timerId);

    auto cloudConnection = std::move(m_cloudConnection);
    cloudConnection.reset();
}

void CloudSystemNameUpdater::update()
{
    NX_LOGX(lit("Peforming out-of-order system name update"), cl_logDEBUG1);
    QnMutexLocker lk(&m_mutex);
    if (m_terminated)
        return;
    NX_ASSERT(m_timerId);
    TimerManager::instance()->modifyTimerDelay(
        m_timerId,
        std::chrono::seconds::zero());
}

void CloudSystemNameUpdater::scheduleNextUpdate(std::chrono::seconds delay)
{
    QnMutexLocker lk(&m_mutex);
    if (m_terminated)
        return;
    m_timerId = TimerManager::instance()->addTimer(
        std::bind(&CloudSystemNameUpdater::updateSystemNameAsync, this),
        delay);
}

void CloudSystemNameUpdater::updateSystemNameAsync()
{
    m_cloudConnection = m_cloudConnectionManager->getCloudConnection();
    const auto cloudCredentials = m_cloudConnectionManager->getSystemCredentials();
    if (!m_cloudConnection || !cloudCredentials)
        return scheduleNextUpdate(kRetrySystemNameUpdatePeriod);

    QnMediaServerResourcePtr server = 
        qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    NX_ASSERT(server);
    if (!server)
        return scheduleNextUpdate(kRetrySystemNameUpdatePeriod);

    NX_LOGX(lit("Updating cloud system name to %1").arg(server->getSystemName()),
        cl_logDEBUG2);

    m_cloudConnection->systemManager()->updateSystemName(
        cloudCredentials->systemId.toStdString(),
        server->getSystemName().toStdString(),
        std::bind(&CloudSystemNameUpdater::systemNameUpdated, this, std::placeholders::_1));
}

void CloudSystemNameUpdater::systemNameUpdated(
    nx::cdb::api::ResultCode resultCode)
{
    NX_LOGX(lm("Cloud system name update finished with result %1")
        .arg(nx::cdb::api::toString(resultCode)),
        resultCode == nx::cdb::api::ResultCode::ok ? cl_logDEBUG2 : cl_logDEBUG1);

    m_cloudConnection.reset();
    scheduleNextUpdate(
        resultCode != nx::cdb::api::ResultCode::ok
        ? kRetrySystemNameUpdatePeriod
        : kSystemNameUpdatePeriod);
}
