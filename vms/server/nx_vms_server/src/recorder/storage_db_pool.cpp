#include "storage_db_pool.h"

#include "media_server/serverutil.h"
#include "utils/common/util.h"
#include <common/common_module.h>
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <nx/utils/log/log.h>
#include <media_server/media_server_module.h>
#include <media_server/settings.h>

QnStorageDbPool::QnStorageDbPool(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
    start();
}

QnStorageDbPool::~QnStorageDbPool()
{
    stop();
}

void QnStorageDbPool::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    {
        QnMutexLocker lock(&m_sdbMutex);
        m_tasksWaitCondition.wakeOne();
    }
}

QnStorageDbPtr QnStorageDbPool::getSDB(const QnStorageResourcePtr &storage)
{
    QnMutexLocker lock( &m_sdbMutex );

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
    {
        if (!(storage->getCapabilities() & QnAbstractStorageResource::cap::WriteFile))
        {
            NX_WARNING(this, lit("%1 Storage %2 is not writable. Can't create storage DB file.")
                    .arg(Q_FUNC_INFO)
                    .arg(storage->getUrl()));
            return sdb;
        }
        QString simplifiedGUID = moduleGUID().toSimpleString();
        QString dbPath = storage->getUrl();
        QString fileName =
            closeDirPath(dbPath) +
            QString::fromLatin1("%1_media.nxdb").arg(simplifiedGUID);

        sdb = QnStorageDbPtr(
            new QnStorageDb(
                serverModule(),
                storage,
                getStorageIndex(storage),
                std::chrono::seconds(
                    serverModule()->roSettings()->value(nx_ms_conf::VACUUM_INTERVAL).toInt())));
        if (sdb->open(fileName)) {
            m_chunksDB[storage->getUrl()] = sdb;
        }
        else {
            NX_WARNING(this, lit("%1 Storage DB file %2 open failed.")
                    .arg(Q_FUNC_INFO)
                    .arg(fileName));
            return QnStorageDbPtr();
        }
    }
    return sdb;
}

int QnStorageDbPool::getStorageIndex(const QnStorageResourcePtr& storage)
{
    QnMutexLocker lock( &m_mutexStorageIndex );

    const QString path = storage->getUrl();
    //QString path = toCanonicalPath(p);

    if (m_storageIndexes.contains(path))
    {
        return *m_storageIndexes.value(path).begin();
    }
    else
    {
        int index = -1;
        for (const QSet<int>& indexes: m_storageIndexes.values())
        {
            for (const int& value: indexes)
                index = qMax(index, value);
        }
        index++;
        m_storageIndexes.insert(path, QSet<int>() << index);
        return index;
    }
}

void QnStorageDbPool::removeSDB(const QnStorageResourcePtr &storage)
{
    QnMutexLocker lk(&m_sdbMutex);
    m_chunksDB.remove(storage->getUrl());
}

void QnStorageDbPool::run()
{
    QnMutexLocker lock(&m_tasksMutex);
    while (!m_needStop)
    {
        while (!needToStop() && m_tasksQueue.empty())
            m_tasksWaitCondition.wait(&m_tasksMutex);

        if (m_needStop)
            return;

        const auto vacuumTask = std::move(m_tasksQueue.front());
        m_tasksQueue.pop();
        lock.unlock();
        vacuumTask();
        lock.relock();
    }
}

void QnStorageDbPool::addVacuumTask(nx::utils::MoveOnlyFunc<void()> vacuumTask)
{
    QnMutexLocker lock(&m_tasksMutex);
    m_tasksQueue.push(std::move(vacuumTask));
    m_tasksWaitCondition.wakeOne();
}