#pragma once

#include <QtCore/QSharedPointer>

#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <nx/utils/uuid.h>

class QnStorageDbPool:
    public QObject,
    public Singleton<QnStorageDbPool>
{
    Q_OBJECT
public:
    QnStorageDbPool(const QnUuid& moduleGuid);

    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    int getStorageIndex(const QnStorageResourcePtr& storage);
    void removeSDB(const QnStorageResourcePtr &storage);

    static QString getLocalGuid(const QnUuid& moduleGuid);
    void flush();
private:
    const QnUuid m_moduleGuid;
    mutable QnMutex m_sdbMutex;
    mutable QnMutex m_mutexStorageIndex;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
    QMap<QString, QSet<int> > m_storageIndexes;
};

#define qnStorageDbPool QnStorageDbPool::instance()
