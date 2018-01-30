#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

struct QnWearableLockInfo
{
    bool locked = false;
    QnUuid userId;
};

class QnWearableLockManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWearableLockManager(QObject* parent = nullptr);
    virtual ~QnWearableLockManager() override;

    bool acquireLock(const QnUuid& cameraId, const QnUuid& token, const QnUuid& userId, qint64 ttl);
    bool extendLock(const QnUuid& cameraId, const QnUuid& token, qint64 ttl);
    bool releaseLock(const QnUuid& cameraId, const QnUuid& token);

    void cleanupExpiredLocks();

    QnWearableLockInfo lockInfo(const QnUuid& cameraId);

private:
    void cleanupExpiredLockUnsafe(const QnUuid& cameraId);

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
