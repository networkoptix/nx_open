#ifndef __STORAGE_DB_H_
#define __STORAGE_DB_H_

#include <QElapsedTimer>
#include "utils/db/db_helper.h"
#include "core/resource/resource.h"
#include "device_file_catalog.h"

class QnStorageDb: public QnDbHelper
{
public:
    QnStorageDb(int storageIndex);
    virtual ~QnStorageDb();

    bool open(const QString& fileName);

    bool deleteRecords(const QByteArray& mac, QnResource::ConnectionRole role, qint64 startTimeMs = -1);
    void addRecord(const QByteArray& mac, QnResource::ConnectionRole role, const DeviceFileCatalog::Chunk& chunk);
    void flushRecords();
    bool createDatabase();
    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    void beforeDelete();
    void afterDelete();
private:
    int m_storageIndex;
    QElapsedTimer m_lastTranTime;

    struct DelayedData 
    {
        DelayedData (const QByteArray& mac = QByteArray(), 
                     QnResource::ConnectionRole role = QnResource::Role_Default, 
                     const DeviceFileCatalog::Chunk& chunk = DeviceFileCatalog::Chunk()): 
        mac(mac), role(role), chunk(chunk) {}

        QByteArray mac;
        QnResource::ConnectionRole role;
        DeviceFileCatalog::Chunk chunk;
    };

    QVector<DelayedData> m_delayedData;
};

typedef QSharedPointer<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
