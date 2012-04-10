#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <QString>
#include <QMap>
#include <QFile>
#include <QMutex>
#include "device_file_catalog.h"
#include "core/resource/storage_resource.h"

class QnAbstractMediaStreamDataProvider;

class QnStorageManager
{
public:
    QnStorageManager();
    virtual ~QnStorageManager();
    static QnStorageManager* instance();
    void addStorage(QnStorageResourcePtr storage);


    QString getFileName(const qint64& fileDate, const QnNetworkResourcePtr netResource, const QString& prefix, QnAbstractMediaStreamDataProvider* provider);
    bool fileStarted(const qint64& startDateMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider);
    bool fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider);

    static QString dateTimeStr(qint64 dateTimeMs);
    QnStorageResourcePtr storageRoot(int storage_index) const { return m_storageRoots.value(storage_index); }
    bool isStorageAvailable(int storage_index) const 
    {
        QnStorageResourcePtr storage = storageRoot(storage_index);
        return storage && storage->getStatus() == QnResource::Online; 
    }

    DeviceFileCatalogPtr getFileCatalog(const QString& mac, QnResource::ConnectionRole role);
    DeviceFileCatalogPtr getFileCatalog(const QString& mac, const QString& qualityPrefix);

    QnTimePeriodList getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel);
    void loadFullFileCatalog();
private:
    QnStorageResourcePtr getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider);
    void clearSpace(QnStorageResourcePtr storage);
    int detectStorageIndex(const QString& path);
    bool deserializeStorageFile();
    bool serializeStorageFile();
    void loadFullFileCatalogInternal(QnResource::ConnectionRole role);
    QnStorageResourcePtr extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& mac, QString& quality);
private:
    typedef QMap<int, QnStorageResourcePtr> StorageMap;
    StorageMap m_storageRoots;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;
    FileCatalogMap m_devFileCatalogHi;
    FileCatalogMap m_devFileCatalogLow;
    mutable QMutex m_mutex;

    QMap<QString, int> m_storageIndexes;
    bool m_storageFileReaded;
};

#define qnStorageMan QnStorageManager::instance()

#endif // __STORAGE_MANAGER_H__
