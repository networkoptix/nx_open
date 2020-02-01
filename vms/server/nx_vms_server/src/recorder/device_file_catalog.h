#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QFileInfo>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/move_only_func.h>

#include <unordered_map>
#include <set>
#include <QtCore/QFileInfo>

#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>
#include "recording/time_period.h"
#include "api/model/recording_stats_reply.h"
#include <nx/vms/server/server_module_aware.h>

#include "chunk.h"
#include "chunks_deque.h"

extern "C" {

#include <libavutil/avutil.h>

} // extern "C"

class QnTimePeriodList;
struct QnTimePeriod;
class QnStorageManager;

using namespace nx::vms::server;

class DeviceFileCatalog: public QObject, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:

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
    nx::vms::server::Chunk updateDuration(
        int durationMs,
        qint64 fileSize,
        bool indexWithDuration,
        qint64 startTimeMs = AV_NOPTS_VALUE);

    qint64 lastChunkStartTime() const;
    qint64 lastChunkStartTime(int storageIndex) const;
    nx::vms::server::Chunk takeChunk(qint64 startTimeMs, qint64 durationMs);

    nx::vms::server::Chunk deleteFirstRecord();

    /** Delete first N records. Return deleted file timestamps */
    QVector<nx::vms::server::Chunk> deleteRecordsBefore(int idx);

    bool isEmpty() const;
    size_t size() const {return m_chunks.size();}
    void clear();
    void deleteRecordsByStorage(int storageIndex, qint64 timeMs);
    int findFileIndex(qint64 startTimeMs, FindMethod method) const;
    int findNextFileIndex(qint64 startTimeMs) const;
    void updateChunkDuration(nx::vms::server::Chunk& chunk);
    QString fileDir(const nx::vms::server::Chunk& chunk) const;
    QString fullFileName(const nx::vms::server::Chunk& chunk) const;
    bool isLastChunk(qint64 startTimeMs) const;
    bool containTime(qint64 timeMs, qint64 eps = 5 * 1000) const;
    bool containTime(const QnTimePeriod& period) const;

    /** Returns AV_NOPTS_VALUE in case if the catalog is empty */
    qint64 minTime() const;

    /** Returns AV_NOPTS_VALUE in case if the catalog is empty */
    qint64 maxTime() const;

    //bool lastFileDuplicateName() const;
    qint64 firstTime() const;
    QnServer::ChunksCatalog getCatalog() const { return m_catalog; }
    //QByteArray getMac() const { return m_macAddress.toUtf8(); }

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

    struct ScanFilter
    {
        ScanFilter() {}
        ScanFilter(const QnTimePeriod& period): scanPeriod(period) {}

        QnTimePeriod scanPeriod;

        bool isEmpty() const { return scanPeriod.durationMs == 0; }
        bool intersects(const QnTimePeriod& period) const;
    };

    void scanMediaFiles(
        const QString& folder,
        const QnStorageResourcePtr &storage,
        QMap<qint64, Chunk>& allChunks,
        QVector<EmptyFileInfo>& emptyFileList,
        const ScanFilter& filter);

    static std::deque<nx::vms::server::Chunk> mergeChunks(
        const std::deque<nx::vms::server::Chunk>& chunks1,
        const std::deque<nx::vms::server::Chunk>& chunks2);

    bool addChunk(const nx::vms::server::Chunk& chunk);
    void addChunks(const nx::vms::server::ChunksDeque& chunks);
    void addChunks(const std::deque<Chunk>& chunk);

    /**
     * Return a deque of chunks such that they exist in *this, but don't exist in the other.
     */
    std::deque<Chunk> setDifference(const DeviceFileCatalog& other) const;

    /**
     * Returns inner deque by copy. Make sure it is what you really want. It is always better to use
     * specialized functions like addChunks(), mergeChunks(), setDifference(), e.t.c.
     */
    ChunksDeque getChunks() const;

    /** Moves inner deque out. Fast but leaves *this in inoperable state. */
    ChunksDeque takeChunks();

    /** Removes all chunks with specified storage index. */
    void removeChunks(int storageIndex);

    /**
     * Merges *this recording statistics with other's. Gets the whole archive statistics except
     * bitrate. It's analyzed for the last records of archive only in
     * range <= bitrateAnalyzePeriodMs.
     * Both catalogs MUST NOT be empty.
     */
    QnRecordingStatsData mergeRecordingStatisticsData(
        const DeviceFileCatalog& other,
        qint64 bitrateAnalyzePeriodMs,
        nx::utils::MoveOnlyFunc<QnStorageResourcePtr(int)> storageRoot) const;

    /** Returns a set all 1st day of the month dates starting from 1/1/1970 present in the catalog. */
    QSet<QDate> recordedMonthList() const;

    /** Returns chunks for those startTimeMs < 'timepointMs' and storageIndex == 'storageIndex' */
    std::deque<Chunk> chunksBefore(int64_t timepointMs, int storageIndex) const;

    void replaceChunks(int storageIndex, const nx::vms::server::ChunksDeque &newCatalog);

    /** Returns chunk at 'index' or default Chunk if 'index' is out of range. */
    nx::vms::server::Chunk chunkAt(int index) const;

    QnServer::ChunksCatalog getRole() const;
    QnRecordingStatsData getStatistics(qint64 bitrateAnalyzePeriodMs) const;

    QnServer::StoragePool getStoragePool() const;
    qint64 getSpaceByStorageIndex(int storageIndex) const;
    bool hasArchive(int storageIndex) const;

    int64_t occupiedSpace(int storageIndex = -1) const;
    std::chrono::milliseconds occupiedDuration(int storageIndex = -1) const;
    std::chrono::milliseconds calendarDuration() const;

    bool hasArchiveRotated() const { return m_hasArchiveRotated; }
    void setHasArchiveRotated(bool value) { m_hasArchiveRotated = value; }
private:

    nx::vms::server::Chunk chunkFromFile(const QnStorageResourcePtr &storage, const QString& fileName);
    QnTimePeriod timePeriodFromDir(const QnStorageResourcePtr &storage, const QString& dirName);
    void removeRecord(int idx);
    int detectTimeZone(qint64 startTimeMs, const QString& fileName);

    QnStorageManager *getMyStorageMan() const;
private:

    mutable QnMutex m_mutex;
    nx::vms::server::ChunksDeque m_chunks;
    const QString m_cameraUniqueId;
    const QnServer::ChunksCatalog m_catalog;
    qint64 m_recordingChunkTime;
    QnMutex m_IOMutex;
    const QnServer::StoragePool m_storagePool;
    mutable int64_t m_lastSyncTime;
    std::atomic<bool> m_hasArchiveRotated = false;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

