#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include <nx/vms/server/server_module_aware.h>

struct QnWearableLockInfo
{
    bool locked = false;
    QnUuid userId;
};

class QnWearableLockManager: public QObject, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnWearableLockManager(QObject* parent);
    virtual ~QnWearableLockManager() override;

    bool acquireLock(const QnUuid& cameraId, const QnUuid& token, const QnUuid& userId, qint64 ttl);
    bool extendLock(const QnUuid& cameraId, const QnUuid& token, qint64 ttl);
    bool releaseLock(const QnUuid& cameraId, const QnUuid& token);

    void cleanupExpiredLocks();

    QnWearableLockInfo lockInfo(const QnUuid& cameraId);

private:
    void cleanupExpiredLockUnsafe(const QnUuid& cameraId);
    void initTimer();
private:
    struct Lock
    {
        QnUuid token;
        QnUuid userId;
        qint64 expiryTime;
    };

    QnMutex m_mutex;
    QHash<QnUuid, Lock> m_lockByCameraId;
};
