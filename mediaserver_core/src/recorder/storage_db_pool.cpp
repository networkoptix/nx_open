#include "storage_db_pool.h"
#include "media_server/serverutil.h"
#include "common/common_module.h"
#include "utils/common/util.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <nx/utils/log/log.h>


namespace
{
    static const QString dbRefFileName( QLatin1String("%1_db_ref.guid") );
}

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

bool QnStorageDbPool::getDBPath( const QnStorageResourcePtr& storage, QString* const dbDirectory )
{
    QString storageUrl = storage->getUrl();
    QString dbRefFilePath;

    //if (storagePath.indexOf(lit("://")) != -1)
    //    dbRefFilePath = dbRefFileName.arg(getLocalGuid());
    //else
    dbRefFilePath = closeDirPath(storageUrl) + dbRefFileName.arg(getLocalGuid());

    QByteArray dbRefGuidStr;
    //checking for file db_ref.guid existence
    if (storage->isFileExists(dbRefFilePath))
    {
        //have to use db from data directory, not from storage
        //reading guid from file
        auto dbGuidFile = std::unique_ptr<QIODevice>(storage->open(dbRefFilePath, QIODevice::ReadOnly));

        if (!dbGuidFile)
            return false;
        dbRefGuidStr = dbGuidFile->readAll();
        //dbGuidFile->close();
    }

    if( !dbRefGuidStr.isEmpty() )
    {
        *dbDirectory = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        if (!QDir().exists(*dbDirectory))
        {
            if(!QDir().mkpath(*dbDirectory))
            {
                qWarning() << "DB path create failed (" << storageUrl << ")";
                return false;
            }
        }
        return true;
    }

    if (storage->getCapabilities() & QnAbstractStorageResource::DBReady)
    {
        *dbDirectory = storageUrl;
        return true;
    }
    else
    {   // we won't be able to create ref guid file if storage is not writable
        if (!(storage->getCapabilities() & QnAbstractStorageResource::WriteFile))
            return false;

        dbRefGuidStr = QUuid::createUuid().toString().toLatin1();
        if( dbRefGuidStr.size() < 2 )
            return false;   //bad guid, somehow
        //removing {}
        dbRefGuidStr.remove( dbRefGuidStr.size()-1, 1 );
        dbRefGuidStr.remove( 0, 1 );
        //saving db ref guid to file on storage
        QIODevice *dbGuidFile = storage->open(
            dbRefFilePath, 
            QIODevice::WriteOnly | QIODevice::Truncate 
            );
        if (dbGuidFile == nullptr)
            return false;
        if( dbGuidFile->write( dbRefGuidStr ) != dbRefGuidStr.size() )
            return false;
        dbGuidFile->close();

        storageUrl = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        if( !QDir().mkpath( storageUrl ) )
        {
            qWarning() << "DB path create failed (" << storageUrl << ")";
            return false;
        }
    }
    *dbDirectory = storageUrl;
    return true;
}

QnStorageDbPtr QnStorageDbPool::getSDB(const QnStorageResourcePtr &storage)
{
    QnMutexLocker lock( &m_sdbMutex );

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb) 
    {
        QString simplifiedGUID = getLocalGuid();
        QString dbPath;
        if( !getDBPath(storage, &dbPath) )
        {
            NX_LOG( lit("Failed to file path to storage DB file. Storage is not writable?"), cl_logWARNING );
            return QnStorageDbPtr();
        }
        //        qWarning() << "DB Path: " << dbPath << "\n";
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
            sdb = QnStorageDbPtr(new QnStorageDb(QnStorageResourcePtr(), getStorageIndex(storage)));
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
