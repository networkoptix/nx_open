#include "schedule_sync.h"

#include <api/global_settings.h>

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

#include <recorder/storage_manager.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <utils/common/util.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/storage_resource.h>

#include <nx/vms/api/types/resource_types.h>

#include <numeric>

using namespace nx;

QnScheduleSync::QnScheduleSync(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule),
    m_syncing(false),
    m_forced(false),
    m_interrupted(false),
    m_failReported(false),
    m_curDow(vms::api::DayOfWeek::none),
    m_syncTimePoint(0),
    m_syncEndTimePoint(0)
{
}

QnScheduleSync::~QnScheduleSync()
{
}

void QnScheduleSync::updateLastSyncChunk()
{
    if (m_syncing)
        return;
    QnMutexLocker lock(&m_syncPointMutex);
    auto chunk = findLastSyncChunkUnsafe();
    m_syncTimePoint = chunk.startTimeMs;
    m_syncEndTimePoint = chunk.endTimeMs();
    NX_VERBOSE(this, lit("[Backup] GetLastSyncPoint: %1").arg(m_syncTimePoint));
}

nx::vms::server::Chunk QnScheduleSync::findLastSyncChunkUnsafe() const
{
    nx::vms::server::Chunk resultChunk;
    nx::vms::server::Chunk prevResultChunk;
    resultChunk.startTimeMs = resultChunk.durationMs
                            = prevResultChunk.startTimeMs
                            = prevResultChunk.durationMs = 0;

    while (auto chunkVector = getOldestChunk(resultChunk.startTimeMs)) {
        auto chunkKey = (*chunkVector)[0];
        prevResultChunk = resultChunk;
        resultChunk = chunkKey.chunk;

        NX_VERBOSE(this, lit("[Backup] Next chunk from DB: %1").arg(resultChunk.startTimeMs));

        auto toCatalog = serverModule()->backupStorageManager()->getFileCatalog(
            chunkKey.cameraId,
            chunkKey.catalog
        );
        if (!toCatalog)
            continue;

        if (toCatalog->lastChunkStartTime() < resultChunk.startTimeMs) {
            resultChunk = prevResultChunk;
            break;
        }
    }
    return resultChunk;
}

QnScheduleSync::ChunkKey QnScheduleSync::getOldestChunk(
    const QString           &cameraId,
    QnServer::ChunksCatalog catalog,
    qint64                  fromTimeMs,
    SyncData                *syncData
) const
{
    auto fromCatalog = serverModule()->normalStorageManager()->getFileCatalog(
        cameraId,
        catalog
    );
    if (!fromCatalog)
    {
        NX_DEBUG(this, lit("[QnScheduleSync::getOldestChunk] get fromCatalog failed"));
        return ChunkKey();
    }
    int nextFileIndex = fromCatalog->findNextFileIndex(fromTimeMs);
    auto chunk = fromCatalog->chunkAt(nextFileIndex);
    if (syncData)
    {
        syncData->currentIndex = syncData->startIndex
                               = nextFileIndex == -1 ? (int)fromCatalog->size() :
                                                            nextFileIndex;
        syncData->totalChunks = (int)fromCatalog->size() - syncData->startIndex;
    }

    ChunkKey ret = {chunk, cameraId, catalog};
    return ret;
}


boost::optional<QnScheduleSync::ChunkKeyVector> QnScheduleSync::getOldestChunk(
    qint64 fromTimeMs) const
{
    ChunkKeyVector ret;
    int64_t minTime = std::numeric_limits<int64_t>::max();

    for (const QnVirtualCameraResourcePtr &camera :
         serverModule()->resourcePool()->getAllCameras(QnResourcePtr(), true))
    {
        Qn::CameraBackupQualities cameraBackupQualities = camera->getActualBackupQualities();

        ChunkKey tmp;

        if (cameraBackupQualities.testFlag(Qn::CameraBackupQuality::CameraBackup_HighQuality))
            tmp = getOldestChunk(camera->getUniqueId(), QnServer::HiQualityCatalog, fromTimeMs);

        if (tmp.chunk.durationMs != -1 && tmp.chunk.startTimeMs != -1)
        {
            if (tmp.chunk.startTimeMs < minTime)
            {
                minTime = tmp.chunk.startTimeMs;
                ret.clear();
                ret.push_back(tmp);
            }
            else if (tmp.chunk.startTimeMs == minTime)
            {
                ret.push_back(tmp);
            }
        }

        if (cameraBackupQualities.testFlag(Qn::CameraBackupQuality::CameraBackup_LowQuality))
            tmp = getOldestChunk(camera->getUniqueId(), QnServer::LowQualityCatalog, fromTimeMs);

        if (tmp.chunk.durationMs != -1 && tmp.chunk.startTimeMs != -1)
        {
            if (tmp.chunk.startTimeMs < minTime)
            {
                minTime = tmp.chunk.startTimeMs;
                ret.clear();
                ret.push_back(tmp);
            }
            else if (tmp.chunk.startTimeMs == minTime)
            {
                ret.push_back(tmp);
            }
        }
    }

    if (ret.empty())
        return boost::none;

    return ret;
}

QnScheduleSync::CopyError QnScheduleSync::copyChunk(const ChunkKey &chunkKey)
{
    auto fromCatalog = serverModule()->normalStorageManager()->getFileCatalog(
        chunkKey.cameraId,
        chunkKey.catalog
    );
    if (!fromCatalog)
        return CopyError::GetCatalogError;

    auto toCatalog = serverModule()->backupStorageManager()->getFileCatalog(
        chunkKey.cameraId,
        chunkKey.catalog
    );
    if (!toCatalog)
        return CopyError::GetCatalogError;

    if (toCatalog->lastChunkStartTime() < chunkKey.chunk.startTimeMs) {
        {   // update sync data
            QnMutexLocker lk(&m_syncDataMutex);
            SyncDataMap::iterator syncDataIt = m_syncData.find(chunkKey);
            NX_ASSERT(syncDataIt != m_syncData.cend());
            auto catalogSize = fromCatalog->size();
            int curFileIndex = fromCatalog->findFileIndex(
                chunkKey.chunk.startTimeMs,
                DeviceFileCatalog::FindMethod::OnRecordHole_NextChunk
            );
            syncDataIt->second.currentIndex = curFileIndex;
            syncDataIt->second.totalChunks = (int) catalogSize - syncDataIt->second.startIndex;
        }

        QString fromFileFullName = fromCatalog->fullFileName(chunkKey.chunk);
        auto fromStorage = QnStorageManager::getStorageByUrl(
            serverModule(),
            fromFileFullName,
            QnServer::StoragePool::Normal
        );

        if (!fromStorage)
            return CopyError::FromStorageError;

        std::unique_ptr<QIODevice> fromFile = std::unique_ptr<QIODevice>(
            fromStorage->open(
                fromFileFullName,
                QIODevice::ReadOnly
            )
        );

        if (!fromFile)
        {   // This means that source storage is not available or
            // source file's been removed by external force.
            if (fromStorage->initOrUpdate() == Qn::StorageInit_Ok)
            {   // File's gone. Log this and skip this file.
                NX_WARNING(this, lit("[Backup::copyFile] Source file %1 open failed. Skipping.")
                        .arg(fromFileFullName));
                return CopyError::SourceFileError;
            }
            else
            {   // Source storage is not available.
                // We will interrupt backup for now and try another time.
                return CopyError::FromStorageError;
            }
        }

        auto relativeFileName = fromFileFullName.mid(fromStorage->getUrl().size());
        auto toStorage = serverModule()->backupStorageManager()->getOptimalStorageRoot();

        if (!toStorage) {
            serverModule()->backupStorageManager()->clearSpace(true);
            toStorage = serverModule()->backupStorageManager()->getOptimalStorageRoot();
            if (!toStorage)
                return CopyError::NoBackupStorageError;
        }

        auto newStorageUrl = toStorage->getUrl();
        auto oldSeparator = getPathSeparator(relativeFileName);
        auto newSeparator = getPathSeparator(newStorageUrl);
        if (oldSeparator != newSeparator)
            relativeFileName.replace(oldSeparator, newSeparator);
        auto newFileName = toStorage->getUrl() + relativeFileName;

        std::unique_ptr<QIODevice> toFile = std::unique_ptr<QIODevice>(
            toStorage->open(
                newFileName,
                QIODevice::WriteOnly
            )
        );

        if (!toFile) {
            NX_WARNING(this, lit("[Backup::copyFile] Target file %1 open failed")
                        .arg(newFileName));
            return CopyError::TargetFileError;
        }

        const int bitrateMBps = m_schedule.backupBitrate; //< Megabytes per second.
        if (bitrateMBps <= 0) // not capped
        {
            auto data = fromFile->readAll();
            toFile->write(data);
        }
        else
        {
            const qint64 fileSize = fromFile->size();

            const qint64 timeToFileMs = (fileSize * 1000) / bitrateMBps;
            const qint64 CHUNK_SIZE = 4096;
            QElapsedTimer writeTimeMs;
            writeTimeMs.restart();

            qint64 dataProcessed = 0;
            while (dataProcessed < fileSize)
            {
                if (m_interrupted || serverModule()->isStopping())
                    return CopyError::Interrupted;

                auto data = fromFile->read(CHUNK_SIZE);
                if (data.isEmpty())
                {
                    NX_WARNING(this, lm("file %1 read error").arg(newFileName));
                    return CopyError::SourceFileError;
                }
                if (toFile->write(data) == -1)
                {
                    NX_WARNING(this, lm("file %1 write error").arg(newFileName));
                    return CopyError::TargetFileError;
                }
                dataProcessed += data.size();
                int intervalMs = (timeToFileMs * dataProcessed) / fileSize;
                int needToSleepMs = intervalMs - writeTimeMs.elapsed();
                if (needToSleepMs > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(needToSleepMs));
            }
        }

        fromFile.reset();
        toFile.reset();

        if (m_interrupted || serverModule()->isStopping()) {
            return CopyError::Interrupted;
        }
        // add chunk to catalog
        bool result = serverModule()->backupStorageManager()->fileStarted(
            chunkKey.chunk.startTimeMs,
            chunkKey.chunk.timeZone,
            newFileName,
            nullptr
        );

        if (!result)
            return CopyError::ChunkError;

        result = serverModule()->backupStorageManager()->fileFinished(
            chunkKey.chunk.durationMs,
            newFileName,
            nullptr,
            chunkKey.chunk.getFileSize()
        );

        if (!result)
            return CopyError::ChunkError;
    }
    return CopyError::NoError;
}

void QnScheduleSync::addSyncDataKey(
    QnServer::ChunksCatalog quality,
    const QString           &cameraId
)
{
    SyncData syncData;
    auto catalog = serverModule()->backupStorageManager()->getFileCatalog(cameraId, quality);
    if (!catalog)
        return;

    ChunkKey tmp = getOldestChunk(
        cameraId,
        quality,
        catalog->lastChunkStartTime(),
        &syncData
    );
    m_syncData.emplace(tmp, syncData);
}

void QnScheduleSync::initSyncData()
{
    for (const QnVirtualCameraResourcePtr &camera :
         serverModule()->resourcePool()->getAllCameras(QnResourcePtr(), true))
    {
        Qn::CameraBackupQualities cameraBackupQualities =
            camera->getActualBackupQualities();

        if (cameraBackupQualities.testFlag(Qn::CameraBackupQuality::CameraBackup_HighQuality))
            addSyncDataKey(QnServer::HiQualityCatalog, camera->getUniqueId());

        if (cameraBackupQualities.testFlag(Qn::CameraBackupQuality::CameraBackup_LowQuality))
            addSyncDataKey(QnServer::LowQualityCatalog, camera->getUniqueId());
    }
}

template<typename NeedMoveOnCB>
vms::api::EventReason QnScheduleSync::synchronize(NeedMoveOnCB needMoveOn)
{
    // Let's check if at least one target backup storage is available first.
    if (!serverModule()->backupStorageManager()->getOptimalStorageRoot())
    {
        NX_DEBUG(this, "[Backup] No approprirate storages found. Bailing out.");
        return vms::api::EventReason::backupFailedNoBackupStorageError;
    }

    NX_DEBUG(this, "[Backup] Starting...");
    QnMutexLocker lock(&m_syncPointMutex);
    auto chunk = findLastSyncChunkUnsafe();
    m_syncTimePoint = chunk.startTimeMs;
    m_syncEndTimePoint = chunk.endTimeMs();

    initSyncData();
    m_syncing = true;

    while (1)
    {
        auto chunkKeyVector = getOldestChunk(m_syncTimePoint);
        if (!chunkKeyVector)
        {
            NX_VERBOSE(this, "[Backup] chunks ended, backup done");
            break;
        }
        m_syncTimePoint = (*chunkKeyVector)[0].chunk.startTimeMs;
        m_syncEndTimePoint = qMax((*chunkKeyVector)[0].chunk.endTimeMs(), m_syncEndTimePoint);
        NX_VERBOSE(this, lit("[Backup] found chunk to backup: %1").arg(m_syncTimePoint));
        for (const auto &chunkKey: *chunkKeyVector)
        {
            auto err = copyChunk(chunkKey);
            if (err != CopyError::NoError && err != CopyError::SourceFileError)
            {
                m_lastError = err;
                NX_WARNING(this, lit("[QnScheduleSync::synchronize] %1").arg(copyErrorString(err)));
                switch (err)
                {
                    case CopyError::NoBackupStorageError:
                    case CopyError::GetCatalogError:
                        return vms::api::EventReason::backupFailedNoBackupStorageError;
                    case CopyError::FromStorageError:
                        return vms::api::EventReason::backupFailedSourceStorageError;
                    case CopyError::SourceFileError:
                        return vms::api::EventReason::backupFailedSourceFileError;
                    case CopyError::TargetFileError:
                        return vms::api::EventReason::backupFailedTargetFileError;
                    case CopyError::ChunkError:
                        return vms::api::EventReason::backupFailedChunkError;
                    case CopyError::Interrupted:
                        return vms::api::EventReason::backupCancelled;
                }
            }
        }
        const auto needMoveOnCode = needMoveOn();
        if (needMoveOnCode != SyncCode::ok)
        {
            NX_DEBUG(this, "Interrupting backup because: '%1'", toString(needMoveOnCode));
            if (needMoveOnCode == SyncCode::wrongTime)
                return vms::api::EventReason::backupEndOfPeriod;
            if (needMoveOnCode == SyncCode::wrongBackupType || needMoveOnCode == SyncCode::interrupted)
                return vms::api::EventReason::backupCancelled;
            break;
        }
    }
    NX_DEBUG(this, "[Backup] successfully finished...");
    m_interrupted = true; // we are done till the next backup period
    return vms::api::EventReason::backupDone;
}

int QnScheduleSync::state() const
{
    int result = Started;
    result |= m_syncing ? Syncing : 0;
    result |= m_forced ? Forced : 0;
    return result;
}

void QnScheduleSync::interrupt()
{
    m_syncing = false;
    m_forced = false;
    m_interrupted = true;
}

void QnScheduleSync::start()
{
    NX_ASSERT(!m_timerManager.hasPendingTasks(), "This method is supposed to be called only once");
    constexpr auto kBackupCheckPeriod = std::chrono::seconds(5);
    m_timerManager.addNonStopTimer(
        [this](auto /*timerId*/) { runCycle(); }, kBackupCheckPeriod, kBackupCheckPeriod);
}

void QnScheduleSync::stop()
{
    m_timerManager.stop();
}

int QnScheduleSync::forceStart()
{
    NX_DEBUG(this, "Start forced");
    if (serverModule()->isStopping())
        return Idle;

    if (m_forced)
        return Forced;

    m_forced = true;
    m_interrupted = false;

    return Ok;
}

QnBackupStatusData QnScheduleSync::getStatus() const
{
    QnMutexLocker lk(&m_syncDataMutex);

    QnBackupStatusData ret;
    ret.state = m_syncing || m_forced ? Qn::BackupState_InProgress : Qn::BackupState_None;
    NX_VERBOSE(
        this, "getStatus: state: %1",
        (ret.state == Qn::BackupState_InProgress ? "'in progress'" : "'none'"));

    ret.backupTimeMs = m_syncEndTimePoint;
    int totalChunks = std::accumulate(
        m_syncData.cbegin(), m_syncData.cend(), 0,
        [](int ac, const SyncDataMap::value_type &p) { return ac + p.second.totalChunks; });

    int processedChunks = std::accumulate(
        m_syncData.cbegin(), m_syncData.cend(), 0,
        [](int ac, const SyncDataMap::value_type &p)
        {
            return ac + p.second.currentIndex - p.second.startIndex;
        });

    ret.progress = (double) processedChunks / (double) (totalChunks == 0 ? 1 : totalChunks);
    return ret;
}

void QnScheduleSync::renewSchedule()
{
    auto server = serverModule()->commonModule()->currentServer();
    NX_ASSERT(server);

    auto oldSchedule = m_schedule;
    if (server)
        m_schedule = server->getBackupSchedule();

    if (m_interrupted && oldSchedule != m_schedule)
        m_interrupted = false; //< Schedule changed, starting from a scratch.
}

QString QnScheduleSync::toString(SyncCode code)
{
    switch (code)
    {
        case SyncCode::ok: return "ok";
        case SyncCode::wrongTime: return "wrong time";
        case SyncCode::interrupted: return "interrupted";
        case SyncCode::wrongBackupType: return "wrong backup type";
        case SyncCode::reindexInProgress: return "reindex in progress";
    }

    return "";
}

void QnScheduleSync::runCycle()
{
    renewSchedule();
    if (m_schedule.backupType == vms::api::BackupType::realtime)
        updateLastSyncChunk();

    auto isItTimeForSync =
        [this] ()
        {
            if (serverModule()->isStopping())
                return SyncCode::interrupted;

            if (m_forced)
                return SyncCode::ok;

            if (m_schedule.backupType != vms::api::BackupType::scheduled)
                return SyncCode::wrongBackupType;

            if (m_schedule.backupDaysOfTheWeek == 0)
                return SyncCode::wrongBackupType;

            QDateTime now = qnSyncTime->currentDateTime();
            const auto today = vms::api::dayOfWeek(Qt::DayOfWeek(now.date().dayOfWeek()));

            if (m_curDow == vms::api::DayOfWeek::none || today != m_curDow)
            {
                m_curDow = today;
                m_interrupted = false; // new day - new life
            }

            if (m_interrupted)
                return SyncCode::interrupted;

            const auto rebuildInfo = serverModule()->normalStorageManager()->rebuildInfo();
            if (rebuildInfo.state != Qn::RebuildState::RebuildState_None)
                return SyncCode::reindexInProgress;

            if (m_schedule.dateTimeFits(now))
                return SyncCode::ok;

            if (m_syncing && m_schedule.isDateTimeBeforeDayPeriodEnd(now))
                return SyncCode::ok;

            return SyncCode::wrongTime;
        };

    const auto shouldStartCode = isItTimeForSync();
    NX_VERBOSE(
        this, "Checking if rebuild might be started. Result is: '%1'",
        toString(shouldStartCode));

    if (shouldStartCode == SyncCode::ok)
    {
         if (m_forced)
            m_failReported = false;

        auto result = synchronize(isItTimeForSync);
        qint64 syncEndTimePointLocal = 0;
        {
            QnMutexLocker lk(&m_syncDataMutex);
            m_syncData.clear();
            syncEndTimePointLocal = m_syncEndTimePoint;
        }
        m_forced = false;
        m_syncing = false;

        bool backupFailed = result != vms::api::EventReason::backupDone
            && result != vms::api::EventReason::backupCancelled
            && result != vms::api::EventReason::backupEndOfPeriod;

        if (backupFailed && !m_failReported)
        {
            emit backupFinished(syncEndTimePointLocal, result);
            m_failReported = true;
        }
        else if (!backupFailed)
        {
            emit backupFinished(syncEndTimePointLocal, result);
            m_failReported = false;
        }
    }
}

bool operator<(const QnScheduleSync::ChunkKey &key1, const QnScheduleSync::ChunkKey &key2)
{
    return key1.cameraId < key2.cameraId ?
           true : key1.cameraId > key2.cameraId ?
                  false : key1.catalog < key2.catalog;
}
