#ifndef _DEVICE_FILE_CATALOG_H___
#define _DEVICE_FILE_CATALOG_H___

#include <QtCore/QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QFileInfo>
#include <nx/utils/thread/mutex.h>

#include <deque>
#include <QtCore/QFileInfo>

#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>
#include "recording/time_period.h"
#include "api/model/recording_stats_reply.h"

class QnTimePeriodList;
class QnTimePeriod;
class QnStorageManager;

class DeviceFileCatalog: public QObject
{
    Q_OBJECT
public:

    struct Chunk
    {
        static const quint16 FILE_INDEX_NONE           = 0xffff;
        static const quint16 FILE_INDEX_WITH_DURATION  = 0xfffe;
        static const int UnknownDuration               = -1;

        Chunk(): startTimeMs(-1), durationMs(0), storageIndex(0), fileIndex(0),timeZone(-1), fileSizeHi(0), fileSizeLo(0) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration, qint16 _timeZone, quint16 fileSizeHi = 0, quint32 fileSizeLo = 0) : 
            startTimeMs(_startTime), durationMs(_duration), storageIndex(_storageIndex), fileIndex(_fileIndex), timeZone(_timeZone), fileSizeHi(fileSizeHi), fileSizeLo(fileSizeLo)
        {
            //Q_ASSERT_X(startTimeMs == -1 || startTimeMs > 0, Q_FUNC_INFO, "Invalid startTime value");
        }

        qint64 distanceToTime(qint64 timeMs) const;
        qint64 endTimeMs() const;
        bool containsTime(qint64 timeMs) const;
        qint64 getFileSize() const { return ((qint64) fileSizeHi << 32) + fileSizeLo; } // 256Tb as max file size
        void setFileSize(qint64 value) { fileSizeHi = quint16(value >> 32); fileSizeLo = quint32(value); } // 256Tb as max file size

        QString fileName() const;


        qint64 startTimeMs; // chunk startTime at ms
        int durationMs; // chunk duration at ms
        quint16 storageIndex;
        quint16 fileIndex;

        qint16 timeZone;
        quint16 fileSizeHi;
        quint32 fileSizeLo;
    };

    struct TruncableChunk : Chunk
    {
        using Chunk::Chunk;
        int64_t originalDuration;
        bool isTruncated;

        TruncableChunk() 
            : Chunk(),
              originalDuration(0),
              isTruncated(false)
        {}

        TruncableChunk(const Chunk &other) 
            : Chunk(other), 
              originalDuration(other.durationMs),
              isTruncated(false)
        {}

        Chunk toBaseChunk() const
        {
            Chunk ret(*this);
            if (originalDuration != 0)
                ret.durationMs = originalDuration;
            return ret;
        }

        void truncate(qint64 timeMs);
    };

    struct EmptyFileInfo
    {
        EmptyFileInfo(): startTimeMs(0) {}
        EmptyFileInfo(qint64 startTimeMs, const QString& fileName): startTimeMs(startTimeMs), fileName(fileName) {}

        qint64 startTimeMs;
        QString fileName;
    };

    // TODO: #Elric #enum
    enum FindMethod {OnRecordHole_NextChunk, OnRecordHole_PrevChunk};

    DeviceFileCatalog(
        const QString           &cameraUniqueId, 
        QnServer::ChunksCatalog catalog, 
        QnServer::StoragePool   role
    );
    //void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    Chunk updateDuration(int durationMs, qint64 fileSize, bool indexWithDuration);
    qint64 lastChunkStartTime() const;
    Chunk takeChunk(qint64 startTimeMs, qint64 durationMs);

    Chunk deleteFirstRecord(); 

    /** Delete first N records. Return deleted file timestamps */
    QVector<Chunk> deleteRecordsBefore(int idx);

    bool isEmpty() const;
    size_t size() const {return m_chunks.size();}
    void clear();
    void deleteRecordsByStorage(int storageIndex, qint64 timeMs);
    int findFileIndex(qint64 startTimeMs, FindMethod method) const;
    int findNextFileIndex(qint64 startTimeMs) const;
    void updateChunkDuration(Chunk& chunk);
    QString fileDir(const Chunk& chunk) const;
    QString fullFileName(const Chunk& chunk) const;
    Chunk chunkAt(int index) const;
    bool isLastChunk(qint64 startTimeMs) const;
    bool containTime(qint64 timeMs, qint64 eps = 5 * 1000) const;
    bool containTime(const QnTimePeriod& period) const;

    qint64 minTime() const;
    qint64 maxTime() const;
    //bool lastFileDuplicateName() const;
    qint64 firstTime() const;
    QnServer::ChunksCatalog getCatalog() const { return m_catalog; }
    //QByteArray getMac() const { return m_macAddress.toUtf8(); }

    // Detail level determine time duration (in microseconds) visible at 1 screen pixel
    // All information less than detail level is discarded
    typedef std::deque<Chunk> ChunkMap;

    QnTimePeriodList getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel, bool keepSmalChunks, int limit);
    void close();

    QString rootFolder(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog) const;
    QString cameraUniqueId() const;

    void setLastSyncTime(int64_t);
    int64_t getLastSyncTime() const;

    static QString prefixByCatalog(QnServer::ChunksCatalog catalog);
    static QnServer::ChunksCatalog catalogByPrefix(const QString &prefix);

    static void cancelRebuildArchive();
    static bool needRebuildPause();
    static void rebuildPause(void*);
    static void rebuildResume(void*);
    static QnMutex m_rebuildMutex;
    static QSet<void*> m_pauseList;

    bool doRebuildArchive(const QnStorageResourcePtr &storage, const QnTimePeriod& period);

    struct ScanFilter
    {
        ScanFilter() {}
        ScanFilter(const QnTimePeriod& period): scanPeriod(period) {}

        QnTimePeriod scanPeriod;

        bool isEmpty() const { return scanPeriod.durationMs == 0; }
        bool intersects(const QnTimePeriod& period) const;
    };

    void scanMediaFiles(const QString& folder, const QnStorageResourcePtr &storage, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList,
        const ScanFilter& filter);

    static std::deque<Chunk> mergeChunks(const std::deque<Chunk>& chunk1, const std::deque<Chunk>& chunk2);
    void addChunks(const std::deque<Chunk>& chunk);
    bool fromCSVFile(const QString& fileName);
    QnServer::ChunksCatalog getRole() const;
    QnRecordingStatsData getStatistics(qint64 bitrateAnalizePeriodMs) const;

    QnServer::StoragePool getStoragePool() const;
    void setStoragePool(QnServer::StoragePool value);
private:

    bool csvMigrationCheckFile(const Chunk& chunk, QnStorageResourcePtr storage);
    bool addChunk(const Chunk& chunk);
    qint64 recreateFile(const QString& fileName, qint64 startTimeMs, const QnStorageResourcePtr &storage);
    QSet<QDate> recordedMonthList();

    void readStorageData(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList, const ScanFilter& scanFilter);
    Chunk chunkFromFile(const QnStorageResourcePtr &storage, const QString& fileName);
    QnTimePeriod timePeriodFromDir(const QnStorageResourcePtr &storage, const QString& dirName);
    void replaceChunks(int storageIndex, const std::deque<Chunk>& newCatalog);
    void removeChunks(int storageIndex);
    void removeRecord(int idx);
    int detectTimeZone(qint64 startTimeMs, const QString& fileName);
    
    friend class QnStorageManager;

    QnStorageManager *getMyStorageMan() const;
private:

    mutable QnMutex m_mutex;
    //QFile m_file;
    std::deque<Chunk> m_chunks; 
    QString m_cameraUniqueId;

    typedef QVector<QPair<int, bool> > CachedDirInfo;
    struct IOCacheEntry
    {
        CachedDirInfo dirInfo;
        QFileInfoList entryList;

        IOCacheEntry() { dirInfo.resize(4); }
    };

    typedef QMap<int, IOCacheEntry > IOCacheMap;
    IOCacheMap m_prevPartsMap[4];

    //bool m_duplicateName;
    //QMap<int,QString> m_prevFileNames;
    const QnServer::ChunksCatalog m_catalog;
    qint64 m_recordingChunkTime;
    QnMutex m_IOMutex;
    QnServer::StoragePool m_storagePool;
    mutable int64_t m_lastSyncTime;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other);
bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other);
bool operator < (const DeviceFileCatalog::Chunk& other, qint64 first);

#endif // _DEVICE_FILE_CATALOG_H__
