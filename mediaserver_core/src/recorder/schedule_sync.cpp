#include "schedule_sync.h"

#include <api/global_settings.h>

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

#include <recorder/storage_manager.h>
#include <nx_ec/data/api_media_server_data.h>
#include <core/resource_management/resource_pool.h>
#include "common/common_module.h"
#include <utils/common/util.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/storage_resource.h>

#include <numeric>

using namespace nx;

QnScheduleSync::QnScheduleSync(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_syncing(false),
    m_forced(false),
    m_interrupted(false),
    m_failReported(false),
    m_curDow(ec2::backup::Never),
    m_syncTimePoint(0),
    m_syncEndTimePoint(0)
{
}

QnScheduleSync::~QnScheduleSync()
{
    stop();
}

void QnScheduleSync::updateLastSyncChunk()
{
    if (m_syncing)
        return;
    QnMutexLocker lock(&m_syncPointMutex);
    auto chunk = findLastSyncChunkUnsafe();
    m_syncTimePoint = chunk.startTimeMs;
    m_syncEndTimePoint = chunk.endTimeMs();
    NX_LOG(lit("[Backup] GetLastSyncPoint: %1").arg(m_syncTimePoint), cl_logDEBUG2);
}

DeviceFileCatalog::Chunk QnScheduleSync::findLastSyncChunkUnsafe() const
{
    DeviceFileCatalog::Chunk resultChunk;
    DeviceFileCatalog::Chunk prevResultChunk;
    resultChunk.startTimeMs = resultChunk.durationMs
                            = prevResultChunk.startTimeMs
                            = prevResultChunk.durationMs = 0;

    while (auto chunkVector = getOldestChunk(resultChunk.startTimeMs)) {
        auto chunkKey = (*chunkVector)[0];
        prevResultChunk = resultChunk;
        resultChunk = chunkKey.chunk;

        NX_LOG(lit("[Backup] Next chunk from DB: %1").arg(resultChunk.startTimeMs),
               cl_logDEBUG2);

        auto toCatalog = qnBackupStorageMan->getFileCatalog(
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
    auto fromCatalog = qnNormalStorageMan->getFileCatalog(
        cameraId,
        catalog
    );
    if (!fromCatalog)
    {
        NX_LOG(
            lit("[QnScheduleSync::getOldestChunk] get fromCatalog failed"),
            cl_logDEBUG1
        );
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


boost::optional<QnScheduleSync::ChunkKeyVector>
QnScheduleSync::getOldestChunk(qint64 fromTimeMs) const
{
    ChunkKeyVector ret;
    int64_t minTime = std::numeric_limits<int64_t>::max();

    for (const QnVirtualCameraResourcePtr &camera : resourcePool()->getAllCameras(QnResourcePtr(), true))
    {
        Qn::CameraBackupQualities cameraBackupQualities = camera->getActualBackupQualities();

        ChunkKey tmp;

        if (cameraBackupQualities.testFlag(Qn::CameraBackup_HighQuality))
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

        if (cameraBackupQualities.testFlag(Qn::CameraBackup_LowQuality))
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
    auto fromCatalog = qnNormalStorageMan->getFileCatalog(
        chunkKey.cameraId,
        chunkKey.catalog
    );
    if (!fromCatalog)
        return CopyError::GetCatalogError;

    auto toCatalog = qnBackupStorageMan->getFileCatalog(
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
                NX_LOG(
                    lit("[Backup::copyFile] Source file %1 open failed. Skipping.")
                        .arg(fromFileFullName),
                    cl_logWARNING
                );
                return CopyError::SourceFileError;
            }
            else
            {   // Source storage is not available.
                // We will interrupt backup for now and try another time.
                return CopyError::FromStorageError;
            }
        }

        auto optimalRootBackupPred = [](const QnStorageResourcePtr & storage) {
            return storage->getFreeSpace() > storage->getSpaceLimit() / 2;
        };
        auto relativeFileName = fromFileFullName.mid(fromStorage->getUrl().size());
        auto toStorage = qnBackupStorageMan->getOptimalStorageRoot(optimalRootBackupPred);

        if (!toStorage) {
            qnBackupStorageMan->clearSpace(true);
            toStorage = qnBackupStorageMan->getOptimalStorageRoot(optimalRootBackupPred);
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
            NX_LOG(lit("[Backup::copyFile] Target file %1 open failed")
                        .arg(newFileName),
                   cl_logWARNING);
            return CopyError::TargetFileError;
        }

        int bitrate = m_schedule.backupBitrate;
        if (bitrate <= 0) // not capped
        {
            auto data = fromFile->readAll();
            toFile->write(data);
        }
        else
        {
            NX_ASSERT(bitrate > 0);
            qint64 fileSize = fromFile->size();
            const qint64 timeToWrite = (fileSize / bitrate) * 1000;

            const qint64 CHUNK_SIZE = 4096;
            const qint64 chunksInFile = std::max((qint64) 1, fileSize / CHUNK_SIZE);
            const qint64 timeOnChunk = timeToWrite / chunksInFile;

            while (fileSize > 0)
            {
                if (m_interrupted || m_needStop) {
                    return CopyError::Interrupted;
                }
                qint64 startTime = qnSyncTime->currentMSecsSinceEpoch();
                const qint64 writeSize = CHUNK_SIZE < fileSize ? CHUNK_SIZE : fileSize;
                auto data = fromFile->read(writeSize);
                if (toFile->write(data) == -1)
                {
                    NX_LOG(
                        lit("[QnScheduleSync::synchronize] file %1 write error")
                            .arg(newFileName),
                        cl_logDEBUG1
                    );
                    return CopyError::TargetFileError;
                }
                fileSize -= writeSize;
                qint64 now = qnSyncTime->currentMSecsSinceEpoch();
                if (now - startTime < timeOnChunk)
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(timeOnChunk - (now - startTime))
                    );
            }
        }

        fromFile.reset();
        toFile.reset();

        if (m_interrupted || m_needStop) {
            return CopyError::Interrupted;
        }
        // add chunk to catalog
        bool result = qnBackupStorageMan->fileStarted(
            chunkKey.chunk.startTimeMs,
            chunkKey.chunk.timeZone,
            newFileName,
            nullptr
        );

        if (!result)
            return CopyError::ChunkError;

        result = qnBackupStorageMan->fileFinished(
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
    auto catalog = qnBackupStorageMan->getFileCatalog(cameraId, quality);
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
         resourcePool()->getAllCameras(QnResourcePtr(), true))
    {
        Qn::CameraBackupQualities cameraBackupQualities =
            camera->getActualBackupQualities();

        if (cameraBackupQualities.testFlag(Qn::CameraBackup_HighQuality))
            addSyncDataKey(QnServer::HiQualityCatalog, camera->getUniqueId());

        if (cameraBackupQualities.testFlag(Qn::CameraBackup_LowQuality))
            addSyncDataKey(QnServer::LowQualityCatalog, camera->getUniqueId());
    }
}

template<typename NeedMoveOnCB>
vms::event::EventReason QnScheduleSync::synchronize(NeedMoveOnCB needMoveOn)
{
    // Let's check if at least one target backup storage is available first.
    if (!qnBackupStorageMan->getOptimalStorageRoot()) {
        NX_LOG("[Backup] No approprirate storages found. Bailing out.", cl_logDEBUG1);
        return vms::event::EventReason::backupFailedNoBackupStorageError;
    }

    NX_LOG("[Backup] Starting...", cl_logDEBUG2);
    QnMutexLocker lock(&m_syncPointMutex);
    auto chunk = findLastSyncChunkUnsafe();
    m_syncTimePoint = chunk.startTimeMs;
    m_syncEndTimePoint = chunk.endTimeMs();

    initSyncData();
    m_syncing = true;

    while (1) {
        auto chunkKeyVector = getOldestChunk(m_syncTimePoint);
        if (!chunkKeyVector) {
            NX_LOG("[Backup] chunks ended, backup done", cl_logDEBUG2);
            break;
        }
        else {
            m_syncTimePoint = (*chunkKeyVector)[0].chunk.startTimeMs;
            m_syncEndTimePoint = qMax((*chunkKeyVector)[0].chunk.endTimeMs(),
                                      m_syncEndTimePoint);
            NX_LOG(lit("[Backup] found chunk to backup: %1").arg(m_syncTimePoint),
                   cl_logDEBUG2);
        }
        for (const auto &chunkKey : *chunkKeyVector) {
            auto err = copyChunk(chunkKey);
            if (err != CopyError::NoError && err != CopyError::SourceFileError) {
                m_lastError = err;
                NX_LOG(
                    lit("[QnScheduleSync::synchronize] %1").arg(copyErrorString(err)),
                    cl_logWARNING
                );
                switch (err) {
                case CopyError::NoBackupStorageError:
                case CopyError::GetCatalogError:
                    return vms::event::EventReason::backupFailedNoBackupStorageError;
                case CopyError::FromStorageError:
                    return vms::event::EventReason::backupFailedSourceStorageError;
                case CopyError::TargetFileError:
                    return vms::event::EventReason::backupFailedTargetFileError;
                case CopyError::ChunkError:
                    return vms::event::EventReason::backupFailedChunkError;
                case CopyError::Interrupted:
                    return vms::event::EventReason::backupCancelled;
                }
            }
        }
        auto needMoveOnCode = needMoveOn();
        bool needStop = needMoveOnCode != SyncCode::Ok;
        if (needStop) {
            if (needMoveOnCode == SyncCode::OutOfTime) {
                return vms::event::EventReason::backupEndOfPeriod;
            } else if (needMoveOnCode == SyncCode::WrongBackupType ||
                       needMoveOnCode == SyncCode::Interrupted) {
                return vms::event::EventReason::backupCancelled;
            }
            break;
        }
    }
    m_interrupted = true; // we are done till the next backup period
    return vms::event::EventReason::backupDone;
}

void QnScheduleSync::stop()
{
    pleaseStop();
    m_forced        = false;
    m_syncing       = false;
    m_interrupted   = true;

    wait();
}

void QnScheduleSync::pleaseStop()
{
    if (isRunning())
    {
        QnLongRunnable::pleaseStop();
        m_stopPromise.set_value();
    }
}

int QnScheduleSync::state() const
{
    int result = 0;
    result |= m_needStop ? Idle : Started;
    result |= m_syncing ? Syncing : 0;
    result |= m_forced ? Forced : 0;
    return result;
}

int QnScheduleSync::interrupt()
{
    if (m_needStop)
        return Idle;
    m_syncing = false;
    m_forced = false;
    m_interrupted = true;
    return Ok;
}

int QnScheduleSync::forceStart()
{
    if (m_needStop)
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
    ret.state = m_syncing || m_forced ? Qn::BackupState_InProgress :
                                        Qn::BackupState_None;
    ret.backupTimeMs = m_syncEndTimePoint;
    int totalChunks = std::accumulate(
        m_syncData.cbegin(),
        m_syncData.cend(),
        0,
        [](int ac, const SyncDataMap::value_type &p)
        {
            return ac + p.second.totalChunks;
        }
    );
    int processedChunks = std::accumulate(
        m_syncData.cbegin(),
        m_syncData.cend(),
        0,
        [](int ac, const SyncDataMap::value_type &p)
        {
            return ac + p.second.currentIndex - p.second.startIndex;
        }
    );
    ret.progress = (double) processedChunks /
                    (double) (totalChunks == 0 ? 1 : totalChunks);
    return ret;
}

void QnScheduleSync::renewSchedule()
{
    auto server = commonModule()->currentServer();
    NX_ASSERT(server);

    auto oldSchedule = m_schedule;
    if (server) {
        m_schedule = server->getBackupSchedule();
    }
    if (m_interrupted) {
        if (oldSchedule != m_schedule) {
            m_interrupted = false; // schedule changed, starting from a scratch
        }
    }
}

void QnScheduleSync::run()
{
    static const auto REDUNDANT_SYNC_TIMEOUT = std::chrono::seconds(5);
    auto stopFuture = m_stopPromise.get_future();

    while (!m_needStop)
    {
        stopFuture.wait_for(REDUNDANT_SYNC_TIMEOUT);


        if (m_needStop)
            return;

        renewSchedule();
        if (m_schedule.backupType == Qn::BackupType::Backup_RealTime)
            updateLastSyncChunk();

        auto isItTimeForSync = [this] ()
        {
            if (m_needStop)
                return SyncCode::Interrupted;
            if (m_forced)
                return SyncCode::Ok;
            if (m_schedule.backupType != Qn::Backup_Schedule)
                return SyncCode::WrongBackupType;

            if (m_schedule.backupDaysOfTheWeek == ec2::backup::Never)
                return SyncCode::WrongBackupType;

            QDateTime now = qnSyncTime->currentDateTime();
            const Qt::DayOfWeek today = static_cast<Qt::DayOfWeek>(now.date().dayOfWeek());

            ec2::backup::DaysOfWeek allowedDays =
                static_cast<ec2::backup::DaysOfWeek>(m_schedule.backupDaysOfTheWeek);

            if (m_curDow == ec2::backup::Never || ec2::backup::fromQtDOW(today) != m_curDow) {
                m_curDow = ec2::backup::fromQtDOW(today);
                m_interrupted = false; // new day - new life
            }

            if (m_interrupted)
                return SyncCode::Interrupted;

            if (allowedDays.testFlag(ec2::backup::fromQtDOW(today)))
            {
                const auto curTime = now.time();
                if (curTime.msecsSinceStartOfDay() > m_schedule.backupStartSec* 1000 &&
                    m_schedule.backupDurationSec == -1) // sync without end time
                {
                    return SyncCode::Ok;
                }

                if (curTime.msecsSinceStartOfDay() > m_schedule.backupStartSec * 1000 &&
                    curTime.msecsSinceStartOfDay() < m_schedule.backupStartSec * 1000 +
                                                        m_schedule.backupDurationSec * 1000) // 'normal' schedule sync
                {
                    return SyncCode::Ok;
                }
            }
            return SyncCode::OutOfTime;
        };

        if (isItTimeForSync() == SyncCode::Ok) // we are there
        {
            while (true)
            {
                bool hasRebuildingStorages = qnNormalStorageMan->hasRebuildingStorages();
                if (hasRebuildingStorages)
                {
                    NX_LOG(
                        lit("[Backup] Can't start because some of the source storages are being rebuilded."),
                        cl_logDEBUG1
                    );
                }
                else if (!hasRebuildingStorages || m_interrupted)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            if (m_interrupted)
                continue;

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

            bool backupFailed = result != vms::event::EventReason::backupDone &&
                                result != vms::event::EventReason::backupCancelled &&
                                result != vms::event::EventReason::backupEndOfPeriod;

            if (backupFailed && !m_failReported) {
                emit backupFinished(syncEndTimePointLocal, result);
                m_failReported = true;
            }
            else if (!backupFailed) {
                emit backupFinished(syncEndTimePointLocal, result);
                m_failReported = false;
            }
        }
    }
}

bool operator < (
    const QnScheduleSync::ChunkKey &key1,
    const QnScheduleSync::ChunkKey &key2
)
{
    return key1.cameraId < key2.cameraId ?
           true : key1.cameraId > key2.cameraId ?
                  false : key1.catalog < key2.catalog;
}
