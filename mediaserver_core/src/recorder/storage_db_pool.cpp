#include "storage_db_pool.h"
#include "media_server/serverutil.h"
#include "common/common_module.h"
#include "utils/common/util.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <nx/utils/log/log.h>

QnStorageDbPool::QnStorageDbPool(): 
    DependedSingleTone<QnStorageDbPool>() 
{

}

QString QnStorageDbPool::getLocalGuid()
{
    QString simplifiedGUID = qnCommon->moduleGUID().toString();
    simplifiedGUID = simplifiedGUID.replace("{", "");
    simplifiedGUID = simplifiedGUID.replace("}", "");
    return simplifiedGUID;
}

QnStorageDbPtr QnStorageDbPool::getSDB(const QnStorageResourcePtr &storage)
{
    QnMutexLocker lock( &m_sdbMutex );

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb) 
    {
        QString simplifiedGUID = getLocalGuid();
        QString dbPath = storage->getUrl();
        QString fileName = closeDirPath(dbPath) + QString::fromLatin1("%1_media.nxdb").arg(simplifiedGUID);
        QString oldFileName = closeDirPath(dbPath) + QString::fromLatin1("media.nxdb");

        if (storage->getCapabilities() & QnAbstractStorageResource::DBReady)
        {
            if (storage->isFileExists(oldFileName) && !storage->isFileExists(fileName))
                storage->renameFile(oldFileName, fileName);

            sdb = QnStorageDbPtr(new QnStorageDb(storage, getStorageIndex(storage)));
        }
        else
        {
            if (QFile::exists(oldFileName) && !QFile::exists(fileName))
                QFile::rename(oldFileName, fileName);
            sdb = QnStorageDbPtr(new QnStorageDb(storage, getStorageIndex(storage)));
        }

        if (sdb->open(fileName)) {
            m_chunksDB[storage->getUrl()] = sdb;
        }
        else {
            qWarning()  << "can't initialize nx media database! File open failed: " << fileName;
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
