#pragma once

#include <QtCore/QSharedPointer>

#include <queue>

#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>
#include <nx/mediaserver/server_module_aware.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/long_runnable.h>

class QnCommonModule;

class QnStorageDbPool :
    public QnLongRunnable,
    public Singleton<QnStorageDbPool>,
    public /*mixin*/ nx::mediaserver::ServerModuleAware
{
    Q_OBJECT
public:
    using VacuumTask = nx::utils::MoveOnlyFunc<void()>;
    QnStorageDbPool(QnMediaServerModule* serverModule);
    ~QnStorageDbPool();

    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    int getStorageIndex(const QnStorageResourcePtr& storage);
    void removeSDB(const QnStorageResourcePtr &storage);
    void addTask(nx::utils::MoveOnlyFunc<void()> vacuumTask);

private:
    mutable QnMutex m_sdbMutex;
    mutable QnMutex m_mutexStorageIndex;
    QnMutex m_tasksMutex;
    QnWaitCondition m_tasksWaitCondition;
    std::queue<nx::utils::MoveOnlyFunc<void()>> m_tasksQueue;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
    QMap<QString, QSet<int> > m_storageIndexes;

    virtual void run() override;
    virtual void pleaseStop() override;
};

#define qnStorageDbPool QnStorageDbPool::instance()