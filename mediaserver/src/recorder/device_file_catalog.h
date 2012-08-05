#ifndef _DEVICE_FILE_CATALOG_H___
#define _DEVICE_FILE_CATALOG_H___

#include <QSharedPointer>
#include <QFile>
#include <QVector>
#include <QMap>
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "recording/time_period.h"
#include <QFileInfo>

class DeviceFileCatalog: public QObject
{
    Q_OBJECT
signals:
    void firstDataRemoved(int n);
public:
    struct Chunk
    {
        Chunk(): startTimeMs(-1), durationMs(0), storageIndex(0), fileIndex(0) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration) : 
            startTimeMs(_startTime), durationMs(_duration), storageIndex(_storageIndex), fileIndex(_fileIndex)
        {
            Q_ASSERT_X(startTimeMs == -1 || startTimeMs > 0, Q_FUNC_INFO, "Invalid startTime value");
        }

        qint64 distanceToTime(qint64 timeMs) const;
        qint64 endTimeMs() const;
        bool containsTime(qint64 timeMs) const;
        void truncate(qint64 timeMs);


        qint64 startTimeMs; // chunk startTime at ms
        int durationMs; // chunk duration at ms

        quint16 storageIndex;
        quint16 fileIndex;
    };

    enum FindMethod {OnRecordHole_NextChunk, OnRecordHole_PrevChunk};

    DeviceFileCatalog(const QString& macAddress, QnResource::ConnectionRole role);
    void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    void updateDuration(int durationMs);
    bool deleteFirstRecord();
    void clear();
    void deleteRecordsBefore(int idx);
    void deleteRecordsBeforeTime(qint64 timeMs);
    void deleteRecordsByStorage(int storageIndex, qint64 timeMs);
    int findFileIndex(qint64 startTimeMs, FindMethod method) const;
    void updateChunkDuration(Chunk& chunk);
    QString fullFileName(const Chunk& chunk) const;
    Chunk chunkAt(int index) const;
    bool isLastChunk(qint64 startTimeMs) const;

    qint64 minTime() const;
    qint64 maxTime() const;
    bool lastFileDuplicateName() const;
    qint64 firstTime() const;
    QnResource::ConnectionRole getRole() const { return m_role; }

    // Detail level determine time duration (in microseconds) visible at 1 screen pixel
    // All information less than detail level is discarded
    typedef QVector<Chunk> ChunkMap;

    QnTimePeriodList getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel);

    static QString prefixForRole(QnResource::ConnectionRole role);
    static QnResource::ConnectionRole roleForPrefix(const QString& prefix);

private:
    bool fileExists(const Chunk& chunk);
    void addChunk(const Chunk& chunk, qint64 lastStartTime);
    qint64 recreateFile(const QString& fileName, qint64 startTimeMs, QnStorageResourcePtr storage);
    QList<QDate> recordedMonthList();
private:
    mutable QMutex m_mutex;
    QFile m_file;
    QVector<Chunk> m_chunks; 
    int m_firstDeleteCount;
    QString m_macAddress;

    QPair<int, bool> m_prevParts[4];
    QFileInfoList m_existFileList;
    bool m_duplicateName;
    QString m_prevFileName;
    QnResource::ConnectionRole m_role;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other);
bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other);
bool operator < (const DeviceFileCatalog::Chunk& other, qint64 first);

#endif // _DEVICE_FILE_CATALOG_H__
