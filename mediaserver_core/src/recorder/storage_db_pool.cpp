#include "storage_db_pool.h"

#include "media_server/serverutil.h"
#include "utils/common/util.h"
#include <common/common_module.h>
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <nx/utils/log/log.h>


QnStorageDbPool::QnStorageDbPool(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
}

QnStorageDbPtr QnStorageDbPool::getSDB(const QnStorageResourcePtr &storage)
{
    QnMutexLocker lock( &m_sdbMutex );

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
    {
        if (!(storage->getCapabilities() & QnAbstractStorageResource::cap::WriteFile))
        {
            NX_LOG(lit("%1 Storage %2 is not writable. Can't create storage DB file.")
                    .arg(Q_FUNC_INFO)
                    .arg(storage->getUrl()), cl_logWARNING);
            return sdb;
        }
        QString simplifiedGUID = commonModule()->moduleGUID().toSimpleString();
        QString dbPath = storage->getUrl();
        QString fileName =
            closeDirPath(dbPath) +
            QString::fromLatin1("%1_media.nxdb").arg(simplifiedGUID);

        sdb = QnStorageDbPtr(new QnStorageDb(serverModule(), storage, getStorageIndex(storage)));
        if (sdb->open(fileName)) {
            m_chunksDB[storage->getUrl()] = sdb;
        }
        else {
            NX_LOG(lit("%1 Storage DB file %2 open failed.")
                    .arg(Q_FUNC_INFO)
                    .arg(fileName), cl_logWARNING);
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
    else {
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

void QnStorageDbPool::flush()
{
    QnMutexLocker lock( &m_sdbMutex );
    for(const QnStorageDbPtr& sdb: m_chunksDB.values())
    {
        if (sdb)
            sdb->flushRecords();
    }
}
