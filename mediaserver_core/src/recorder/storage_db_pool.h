#pragma once

#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <QSharedPointer>
#include <common/common_module_aware.h>

class QnStorageDbPool:
    public QObject,
    public Singleton<QnStorageDbPool>,
    public QnCommonModuleAware
{
    Q_OBJECT
public:
    QnStorageDbPool(QnCommonModule* commonModule);

    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    int getStorageIndex(const QnStorageResourcePtr& storage);
    void removeSDB(const QnStorageResourcePtr &storage);

    static QString getLocalGuid(QnCommonModule* commonModule);
    void flush();
private:
    mutable QnMutex m_sdbMutex;
    mutable QnMutex m_mutexStorageIndex;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
    QMap<QString, QSet<int> > m_storageIndexes;
};

#define qnStorageDbPool QnStorageDbPool::instance()
