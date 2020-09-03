#pragma once

#include "storage_check_runner.h"

namespace nx::vms::server {

using namespace std::chrono;

void StorageCheckRunner::forceCheck()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!NX_ASSERT(m_timerId))
        return;

    m_timerManager.deleteTimer(*m_timerId);
    m_timerManager.addTimer([this](nx::utils::TimerId) { m_task(); }, 0ms);
    m_timerId = m_timerManager.addNonStopTimer(
        [this](nx::utils::TimerId) { m_task(); },
        m_taskPeriod, m_taskPeriod);
}

void StorageCheckRunner::start(
    nx::utils::MoveOnlyFunc<void()> task, std::chrono::milliseconds taskPeriod)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(!m_task, "This method is supposed to be called only once and before all others");
    m_task = std::move(task);
    m_taskPeriod = taskPeriod;
    m_timerId = m_timerManager.addNonStopTimer(
        [this](nx::utils::TimerId) { m_task();},
        taskPeriod, taskPeriod);
}

void StorageCheckRunner::stop()
{
    m_timerManager.stop();
}

StorageCheckRunner::~StorageCheckRunner()
{
    stop();
}

} // namespace nx::vms::server