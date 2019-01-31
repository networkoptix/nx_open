#pragma once

#include <QtCore/QSharedPointer>

#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>
#include <nx/vms/server/server_module_aware.h>

class QnCommonModule;

class QnStorageDbPool:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnStorageDbPool(QnMediaServerModule* serverModule);

    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    int getStorageIndex(const QnStorageResourcePtr& storage);
    void removeSDB(const QnStorageResourcePtr &storage);

    void flush();
private:
    mutable QnMutex m_sdbMutex;
    mutable QnMutex m_mutexStorageIndex;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
    QMap<QString, QSet<int> > m_storageIndexes;
};
