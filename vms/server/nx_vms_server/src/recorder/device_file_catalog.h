#pragma once

#include <QtCore/QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QFileInfo>
#include <nx/utils/thread/mutex.h>

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
    nx::vms::server::Chunk chunkAt(int index) const;
    bool isLastChunk(qint64 startTimeMs) const;
    bool containTime(qint64 timeMs, qint64 eps = 5 * 1000) const;
    bool containTime(const QnTimePeriod& period) const;

    qint64 minTime() const;
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
    void addChunks(const nx::vms::server::ChunksDeque& chunks);
    void addChunks(const std::deque<Chunk>& chunk);

    void assignChunksUnsafe(
        std::deque<Chunk>::const_iterator begin,
        std::deque<Chunk>::const_iterator end);

    bool fromCSVFile(const QString& fileName);
    QnServer::ChunksCatalog getRole() const;
    QnRecordingStatsData getStatistics(qint64 bitrateAnalyzePeriodMs) const;

    QnServer::StoragePool getStoragePool() const;
    qint64 getSpaceByStorageIndex(int storageIndex) const;
    bool hasArchive(int storageIndex) const;

    // only for unit tests, don't use in production.
    const std::deque<Chunk>& getChunksUnsafe() const { return m_chunks.chunks(); }

    int64_t occupiedSpace(int storageIndex = -1) const;
    std::chrono::milliseconds occupiedDuration(int storageIndex = -1) const;
    std::chrono::milliseconds calendarDuration(int storageIndex = -1) const;

    bool hasRemovedFile() const { return m_hasRemovedFile; }
    void setHasRemovedFile(bool value) { m_hasRemovedFile = value; }
private:

    bool csvMigrationCheckFile(const nx::vms::server::Chunk& chunk, QnStorageResourcePtr storage);
    bool addChunk(const nx::vms::server::Chunk& chunk);
    QSet<QDate> recordedMonthList();

    nx::vms::server::Chunk chunkFromFile(const QnStorageResourcePtr &storage, const QString& fileName);
    QnTimePeriod timePeriodFromDir(const QnStorageResourcePtr &storage, const QString& dirName);
    void replaceChunks(int storageIndex, const nx::vms::server::ChunksDeque &newCatalog);
    void removeChunks(int storageIndex);
    void removeRecord(int idx);
    int detectTimeZone(qint64 startTimeMs, const QString& fileName);

    friend class QnStorageManager;

    QnStorageManager *getMyStorageMan() const;
private:

    mutable QnMutex m_mutex;
    //QFile m_file;
    nx::vms::server::ChunksDeque m_chunks;
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
    std::atomic<bool> m_hasRemovedFile = false;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

