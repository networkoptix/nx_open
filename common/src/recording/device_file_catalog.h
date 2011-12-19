#ifndef _DEVICE_FILE_CATALOG_H__
#define _DEVICE_FILE_CATALOG_H__

#include <QSharedPointer>
#include <QFile>
#include <QVector>
#include <QMap>
#include <QSharedPointer>
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"

struct QnTimePeriod;
typedef QVector<QnTimePeriod> QnTimePeriodList;

struct QnTimePeriod
{
    QnTimePeriod(): startTimeMs(0), durationMs(0) {}
    QnTimePeriod(qint64 _startTimeMs, int _durationMs): startTimeMs(_startTimeMs), durationMs(_durationMs) {}

    static QnTimePeriodList mergeTimePeriods(QVector<QnTimePeriodList> periods);

    bool containTime(qint64 timeMs) const;

    /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds. 
     * 
     * -1 if duration is infinite or unknown. It may be the case if this time period 
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;

};
bool operator < (const QnTimePeriod& first, const QnTimePeriod& other);
bool operator < (qint64 first, const QnTimePeriod& other);
bool operator < (const QnTimePeriod& other, qint64 first);


class DeviceFileCatalog: public QObject
{
    Q_OBJECT
signals:
    void firstDataRemoved(int n);
public:
    struct Chunk
    {
        Chunk(): startTimeMs(-1), storageIndex(0), fileIndex(0), durationMs(0) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration) : 
            startTimeMs(_startTime), storageIndex(_storageIndex), fileIndex(_fileIndex), durationMs(_duration) {}

        qint64 startTimeMs; // chunk startTime at ms
        quint16 storageIndex;
        quint16 fileIndex;
        int durationMs; // chunk duration at ms
    };

    enum FindMethod {OnRecordHole_NextChunk, OnRecordHole_PrevChunk};

    DeviceFileCatalog(const QString& macAddress);
    void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    void updateDuration(int durationMs);
    bool deleteFirstRecord();
    int findFileIndex(qint64 startTimeMs, FindMethod method) const;
    QString fullFileName(const Chunk& chunk) const;
    Chunk chunkAt(int index) const;
    qint64 minTime() const;
    qint64 maxTime() const;
    bool fileExists(const Chunk& chunk);
    bool lastFileDuplicateName() const;
    qint64 firstTime() const;

    // Detail level determine time duration (in microseconds) visible at 1 screen pixel
    // All information less than detail level is discarded
    typedef QVector<Chunk> ChunkMap;

    QnTimePeriodList getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel);
private:
    qint64 recreateFile(const QString& fileName, qint64 startTimeMs);
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
