#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <QString>
#include <QMap>
#include <QFile>
#include <QMutex>

#include "recording/time_period_list.h"
#include "device_file_catalog.h"
#include "core/resource/storage_resource.h"

class QnAbstractMediaStreamDataProvider;

class QnStorageManager: public QObject
{
    Q_OBJECT
public:
    typedef QMap<int, QnStorageResourcePtr> StorageMap;

    QnStorageManager();
    virtual ~QnStorageManager();
    static QnStorageManager* instance();
    QnStorageResourcePtr removeStorage(QnStorageResourcePtr storage);

    /*
    * Remove storage if storage is absent in specified list
    */
    void removeAbsentStorages(QnAbstractStorageResourceList newStorages);
    void addStorage(QnStorageResourcePtr storage);


    QString getFileName(const qint64& fileDate, qint16 timeZone, const QnNetworkResourcePtr netResource, const QString& prefix, QnStorageResourcePtr& storage);
    bool fileStarted(const qint64& startDateMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider* provider);
    bool fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider,  qint64 fileSize);

    /*
    * convert UTC time to folder name. Used for server archive catalog.
    * @param dateTimeMs UTC time in ms
    * timeZone media server time zone offset in munutes. If value==-1 - current(system) time zone is used
    */
    static QString dateTimeStr(qint64 dateTimeMs, qint16 timeZone);

    QnStorageResourcePtr getStorageByUrl(const QString& fileName);
    QnStorageResourcePtr storageRoot(int storage_index) const { QMutexLocker lock(&m_mutexStorages); return m_storageRoots.value(storage_index); }
    bool isStorageAvailable(int storage_index) const 
    {
        QnStorageResourcePtr storage = storageRoot(storage_index);
        return storage && storage->getStatus() == QnResource::Online; 
    }

    DeviceFileCatalogPtr getFileCatalog(const QString& mac, QnResource::ConnectionRole role);
    DeviceFileCatalogPtr getFileCatalog(const QString& mac, const QString& qualityPrefix);

    QnTimePeriodList getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel);
    void loadFullFileCatalog();
    QnStorageResourcePtr getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider);

    StorageMap getAllStorages() const { QMutexLocker lock(&m_mutexStorages); return m_storageRoots; }
public slots:
    void at_archiveRangeChanged(qint64 newStartTimeMs, qint64 newEndTimeMs);
private:
    void clearSpace(QnStorageResourcePtr storage);
    int detectStorageIndex(const QString& path);
    bool deserializeStorageFile();
    bool serializeStorageFile();
    void loadFullFileCatalogInternal(QnResource::ConnectionRole role);
    QnStorageResourcePtr extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& mac, QString& quality);
    void getTimePeriodInternal(QVector<QnTimePeriodList>& cameras, QnNetworkResourcePtr camera, qint64 startTime, qint64 endTime, qint64 detailLevel, DeviceFileCatalogPtr catalog);
    bool existsStorageWithID(const QnAbstractStorageResourceList& storages, QnId id) const;
    void updateStorageStatistics();
private:
    StorageMap m_storageRoots;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;
    FileCatalogMap m_devFileCatalogHi;
    FileCatalogMap m_devFileCatalogLow;
    mutable QMutex m_mutexStorages;
    mutable QMutex m_mutexCatalog;

    QMap<QString, int> m_storageIndexes;
    bool m_storageFileReaded;
    bool m_storagesStatisticsReady;
};

#define qnStorageMan QnStorageManager::instance()

#endif // __STORAGE_MANAGER_H__
