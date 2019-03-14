#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QFileInfo>
#include <nx/utils/thread/mutex.h>

#include <deque>
#include <unordered_map>
#include <set>
#include <QtCore/QFileInfo>

#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>
#include "recording/time_period.h"
#include "api/model/recording_stats_reply.h"
#include <nx/vms/server/server_module_aware.h>

extern "C" {

#include <libavutil/avutil.h>

} // extern "C"

class QnTimePeriodList;
struct QnTimePeriod;
class QnStorageManager;

class DeviceFileCatalog: public QObject, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:

    struct Chunk
    {
        static const quint16 FILE_INDEX_NONE;
        static const quint16 FILE_INDEX_WITH_DURATION;
        static const int UnknownDuration;

        Chunk(): startTimeMs(-1), durationMs(0), storageIndex(0), fileIndex(0),timeZone(-1), fileSizeHi(0), fileSizeLo(0) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration, qint16 _timeZone, quint16 fileSizeHi = 0, quint32 fileSizeLo = 0) :
            startTimeMs(_startTime), durationMs(_duration), storageIndex(_storageIndex), fileIndex(_fileIndex), timeZone(_timeZone), fileSizeHi(fileSizeHi), fileSizeLo(fileSizeLo)
        {
            //NX_ASSERT(startTimeMs == -1 || startTimeMs > 0, "Invalid startTime value");
        }

        qint64 distanceToTime(qint64 timeMs) const;
        qint64 endTimeMs() const;
        bool containsTime(qint64 timeMs) const;
        qint64 getFileSize() const { return ((qint64) fileSizeHi << 32) + fileSizeLo; } // 256Tb as max file size
        void setFileSize(qint64 value) { fileSizeHi = quint16(value >> 32); fileSizeLo = quint32(value); } // 256Tb as max file size
        bool isInfinite() const { return durationMs == -1; }

        QString fileName() const;

        qint64 startTimeMs; // chunk startTime at ms
        int durationMs; // chunk duration at ms
        quint16 storageIndex;
        quint16 fileIndex;

        qint16 timeZone;
        quint16 fileSizeHi;
        quint32 fileSizeLo;
    };

    class ChunksDeque: public std::deque<Chunk>
    {
    public:
        using iterator = std::deque<Chunk>::iterator;
        using const_iterator = std::deque<Chunk>::const_iterator;
        using Base = std::deque<Chunk>;

        iterator insert(const_iterator pos, const Chunk& chunk)
        {
            chunkAdded(chunk.storageIndex);
            return Base::insert(pos, chunk);
        }

        iterator insert(const_iterator pos, Chunk&& chunk)
        {
            chunkAdded(chunk.storageIndex);
            return Base::insert(pos, std::move(chunk));
        }

        iterator insert(iterator pos, const Chunk& chunk)
        {
            chunkAdded(chunk.storageIndex);
            return Base::insert(pos, chunk);
        }

        void push_back(const Chunk& chunk)
        {
            chunkAdded(chunk.storageIndex);
            Base::push_back(chunk);
        }

        void push_back(Chunk&& chunk)
        {
            chunkAdded(chunk.storageIndex);
            Base::push_back(std::move(chunk));
        }

        template< class... Args >
        iterator emplace(const_iterator pos, Args&&... args) = delete;

        template< class... Args >
        void emplace_front(Args&&... args) = delete;

        template< class... Args >
        Base::reference emplace_front(Args&&... args) = delete;

        template< class... Args >
        void emplace_back(Args&&... args) = delete;

        template< class... Args >
        Base::reference emplace_back(Args&&... args) = delete;

        void pop_back() = delete;
        void push_front(const Chunk& value) = delete;
        void push_front(Chunk&& value) = delete;

        void pop_front()
        {
            chunkRemoved(front().storageIndex);
            Base::pop_front();
        }

        iterator erase(iterator pos)
        {
            chunkRemoved(pos->storageIndex);
            return Base::erase(pos);
        }

        iterator erase(const_iterator pos)
        {
            chunkRemoved(pos->storageIndex);
            return Base::erase(pos);
        }

        void clear() noexcept = delete;

        iterator erase(iterator first, iterator last) = delete;
        iterator erase(const_iterator first, const_iterator last) = delete;

        bool hasArchive(int storageIndex) const
        {
            return m_archivePresenceMap.find(storageIndex) != m_archivePresenceMap.cend();
        }

        void reCalcArchivePresence()
        {
            m_archivePresenceMap.clear();
            for (auto it = Base::cbegin(); it != Base::cend(); ++it)
                chunkAdded(it->storageIndex);
        }

    private:
        std::map<int, int> m_archivePresenceMap;

        void chunkAdded(int storageIndex)
        {
            m_archivePresenceMap[storageIndex]++;
        }

        void chunkRemoved(int storageIndex)
        {
            auto it = m_archivePresenceMap.find(storageIndex);
            if (it == m_archivePresenceMap.cend())
                return;

            if (--it->second <= 0)
                m_archivePresenceMap.erase(it);
        }
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

    struct UniqueChunk
    {
        TruncableChunk              chunk;
        QString                     cameraId;
        QnServer::ChunksCatalog     quality;
        bool                        isBackup;

        UniqueChunk(
            const TruncableChunk        &chunk,
            const QString               &cameraId,
            QnServer::ChunksCatalog     quality,
            bool                        isBackup
        )
          : chunk(chunk),
            cameraId(cameraId),
            quality(quality),
            isBackup(isBackup)
        {}
    };

    typedef std::set<UniqueChunk> UniqueChunkCont;

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
        QnMediaServerModule* serverModule,
        const QString&           cameraUniqueId,
        QnServer::ChunksCatalog catalog,
        QnServer::StoragePool   role
    );
    //void deserializeTitleFile();
    void addRecord(const Chunk& chunk, bool sideRecorder = false);
    Chunk updateDuration(
        int durationMs,
        qint64 fileSize,
        bool indexWithDuration,
        qint64 startTimeMs = AV_NOPTS_VALUE);

    qint64 lastChunkStartTime() const;
    qint64 lastChunkStartTime(int storageIndex) const;
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

    QnTimePeriodList getTimePeriods(
        qint64 startTimeMs,
        qint64 endTimeMs,
        qint64 detailLevel,
        bool keepSmalChunks,
        int limit,
        Qt::SortOrder sortOrder);
    void close();

    QString rootFolder(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog) const;
    QString cameraUniqueId() const;

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

    template<typename It>
    void assignChunksUnsafe(It begin, It end) { m_chunks.assign(begin, end); }

    bool fromCSVFile(const QString& fileName);
    QnServer::ChunksCatalog getRole() const;
    QnRecordingStatsData getStatistics(qint64 bitrateAnalyzePeriodMs) const;

    QnServer::StoragePool getStoragePool() const;
    qint64 getSpaceByStorageIndex(int storageIndex) const;
    bool hasArchive(int storageIndex) const;

    // only for unit tests, don't use in production.
    std::deque<Chunk> &getChunksUnsafe() { return m_chunks; }
private:

    bool csvMigrationCheckFile(const Chunk& chunk, QnStorageResourcePtr storage);
    bool addChunk(const Chunk& chunk);
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
    ChunksDeque m_chunks;
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
    const QnServer::StoragePool m_storagePool;
    mutable int64_t m_lastSyncTime;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other);
bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other);
bool operator < (const DeviceFileCatalog::Chunk& other, qint64 first);
bool operator == (const DeviceFileCatalog::Chunk &lhs, const DeviceFileCatalog::Chunk &rhs);

inline bool operator == (const DeviceFileCatalog::UniqueChunk    &lhs,
                         const DeviceFileCatalog::UniqueChunk    &rhs)
{
    return lhs.chunk.toBaseChunk().startTimeMs == rhs.chunk.toBaseChunk().startTimeMs &&
           lhs.chunk.toBaseChunk().durationMs == rhs.chunk.toBaseChunk().durationMs &&
           lhs.cameraId == rhs.cameraId && lhs.quality == rhs.quality &&
           lhs.isBackup == rhs.isBackup;
}

inline bool operator < (const DeviceFileCatalog::UniqueChunk    &lhs,
                        const DeviceFileCatalog::UniqueChunk    &rhs)
{
    if (lhs.cameraId != rhs.cameraId || lhs.quality != rhs.quality || lhs.isBackup != rhs.isBackup) {
        return lhs.cameraId < rhs.cameraId ?
               true : lhs.cameraId > rhs.cameraId ?
                      false : lhs.quality < rhs.quality ?
                            true : lhs.quality > rhs.quality ?
                                false : lhs.isBackup < rhs.isBackup;
    } else {
        return lhs.chunk.startTimeMs < rhs.chunk.startTimeMs;
    }
}
