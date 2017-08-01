#pragma once

#include <QtCore/QSharedPointer>

#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <nx/utils/uuid.h>

class QnCommonModule;

class QnStorageDbPool:
    public QObject,
    public Singleton<QnStorageDbPool>
{
    Q_OBJECT
public:
    QnStorageDbPool(QnCommonModule* commonModule);

    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    int getStorageIndex(const QnStorageResourcePtr& storage);
    void removeSDB(const QnStorageResourcePtr &storage);

    void flush();
private:
    mutable QnMutex m_sdbMutex;
    mutable QnMutex m_mutexStorageIndex;
    QnCommonModule* m_commonModule = nullptr;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
    QMap<QString, QSet<int> > m_storageIndexes;
};

#define qnStorageDbPool QnStorageDbPool::instance()
