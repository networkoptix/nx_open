#include "wearable_lock_manager.h"

#include <QtCore/QTimer>

#include <utils/common/synctime.h>
#include <core/resource_management/resource_pool.h>

#include "media_server_module.h"

namespace {
/**
 * In all sane scenarios cleaning up expired locks won't be needed,
 * but we're being extra meticulous just in case.
 */
const int kCleanupPeriodMSec = 1000 * 60 * 10;
}

QnWearableLockManager::QnWearableLockManager(QObject* parent): base_type(parent)
{
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnWearableLockManager::cleanupExpiredLocks);
    timer->start(kCleanupPeriodMSec);
}

QnWearableLockManager::~QnWearableLockManager()
{
}

bool QnWearableLockManager::acquireLock(const QnUuid& cameraId, const QnUuid& token, const QnUuid& userId, qint64 ttl)
{

    QnMutexLocker locker(&m_mutex);

    cleanupExpiredLockUnsafe(cameraId);

    auto pos = m_lockByCameraId.find(cameraId);
    if (m_lockByCameraId.contains(cameraId))
        return false;

    Lock lock;
    lock.userId = userId;
    lock.token = token;
    lock.expiryTime = qnSyncTime->currentMSecsSinceEpoch() + ttl;
    m_lockByCameraId.insert(cameraId, lock);

    return true;
}

bool QnWearableLockManager::extendLock(const QnUuid& cameraId, const QnUuid& token, qint64 ttl)
{
    QnMutexLocker locker(&m_mutex);

    cleanupExpiredLockUnsafe(cameraId);

    auto pos = m_lockByCameraId.find(cameraId);
    if (pos == m_lockByCameraId.end())
        return false;

    Lock& lock = *pos;
    if (lock.token != token)
        return false;

    lock.expiryTime = std::max(lock.expiryTime, qnSyncTime->currentMSecsSinceEpoch() + ttl);
    return true;
}

bool QnWearableLockManager::releaseLock(const QnUuid& cameraId, const QnUuid& token)
{
    QnMutexLocker locker(&m_mutex);

    cleanupExpiredLockUnsafe(cameraId);

    auto pos = m_lockByCameraId.find(cameraId);
    if (pos == m_lockByCameraId.end())
        return false;

    Lock& lock = *pos;
    if (lock.token != token)
        return false;

    m_lockByCameraId.erase(pos);
    return true;
}

void QnWearableLockManager::cleanupExpiredLocks()
{
    for (const QnUuid cameraId : m_lockByCameraId.keys())
        cleanupExpiredLockUnsafe(cameraId);
}

void QnWearableLockManager::cleanupExpiredLockUnsafe(const QnUuid& cameraId)
{
    auto pos = m_lockByCameraId.find(cameraId);
    if (pos == m_lockByCameraId.end())
        return;

    // Check ttl.
    if (pos->expiryTime <= qnSyncTime->currentMSecsSinceEpoch())
    {
        m_lockByCameraId.erase(pos);
        return;
    }

    // Check if user was deleted.
    if (!qnServerModule->commonModule()->resourcePool()->getResourceById(pos->userId))
        m_lockByCameraId.erase(pos);
}

QnWearableLockInfo QnWearableLockManager::lockInfo(const QnUuid& cameraId)
{
    QnMutexLocker locker(&m_mutex);

    cleanupExpiredLockUnsafe(cameraId);

    QnWearableLockInfo result;

    auto pos = m_lockByCameraId.find(cameraId);
    if (pos == m_lockByCameraId.end())
        return result;

    result.locked = true;
    result.userId = pos->userId;
    return result;
}
