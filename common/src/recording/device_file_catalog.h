#ifndef _DEVICE_FILE_CATALOG_H__
#define _DEVICE_FILE_CATALOG_H__

#include <QSharedPointer>
#include <QFile>
#include <QVector>
#include <QMap>
#include <QSharedPointer>
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"

struct QnTimePeriod
{
    QnTimePeriod(): startTime(0), duration(0) {}
    QnTimePeriod(qint64 _startTime, qint64 _duration): startTime(_startTime), duration(_duration) {}

    qint64 startTime;
    qint64 duration;
};
typedef QVector<QnTimePeriod> QnTimePeriodList;

class DeviceFileCatalog: public QObject
{
    Q_OBJECT
signals:
    void firstDataRemoved(int n);
public:
    struct Chunk
    {
        Chunk(): startTime(-1), storageIndex(0), fileIndex(0), duration(0) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration) : 
            startTime(_startTime), storageIndex(_storageIndex), fileIndex(_fileIndex), duration(_duration) {}

        qint64 startTime;
        quint16 storageIndex;
        quint16 fileIndex;
        int duration; // chunk duration at ms
    };

    DeviceFileCatalog(const QString& macAddress);
    void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    void updateDuration(int duration);
    bool deleteFirstRecord();
    int findFileIndex(qint64 startTime) const;
    QString fullFileName(const Chunk& chunk) const;
    Chunk chunkAt(int index) const;
    qint64 minTime() const;
    qint64 maxTime() const;
    bool fileExists(const Chunk& chunk);
    bool lastFileDuplicateName() const;
    qint64 firstTime() const;

    // Detail level determine time duration (at mks) visible at 1 screen pixel
    // All infornation less than detail level is discarded
    typedef QVector<Chunk> ChunkMap;

    QnTimePeriodList getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel);
private:
    qint64 getFileDuration(const QString& fileName);
private:
    mutable QMutex m_mutex;
    QFile m_file;
    QVector<Chunk> m_chunks; 
    int m_firstDeleteCount;
    QString m_macAddress;

    QPair<int, bool> m_prevParts[4];
    QStringList m_existFileList;
    bool m_duplicateName;
    QString m_prevFileName;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other);
bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other);
bool operator < (const DeviceFileCatalog::Chunk& other, qint64 first);

#endif // _DEVICE_FILE_CATALOG_H__
