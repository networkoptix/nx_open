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

QnScheduleSync::QnScheduleSync()
    : m_backupSyncOn(false),
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
            chunkKey.cameraID,
            chunkKey.catalog
        );
        if (!toCatalog)
            continue;

        if (toCatalog->getLastSyncTime() < resultChunk.startTimeMs) {
            resultChunk = prevResultChunk;
            break;
        }
    }
    return resultChunk;
}

QnScheduleSync::ChunkKey QnScheduleSync::getOldestChunk(
    const QString           &cameraId,
    QnServer::ChunksCatalog catalog,
    qint64                  fromTimeMs
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

    ChunkKey ret = {chunk, cameraId, catalog};
    return ret;
}


boost::optional<QnScheduleSync::ChunkKeyVector> 
QnScheduleSync::getOldestChunk(qint64 fromTimeMs) const
{
    ChunkKeyVector ret;
    int64_t minTime = std::numeric_limits<int64_t>::max();

    for (const QnVirtualCameraResourcePtr &camera : qnResPool->getAllCameras(QnResourcePtr(), true)) 
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
        chunkKey.cameraID,
        chunkKey.catalog
    );
    if (!fromCatalog)
        return CopyError::GetCatalogError;

    auto toCatalog = qnBackupStorageMan->getFileCatalog(
        chunkKey.cameraID,
        chunkKey.catalog
    );
    if (!toCatalog)
        return CopyError::GetCatalogError;

    if (toCatalog->getLastSyncTime() < chunkKey.chunk.startTimeMs) {
        {   // update sync data
            QnMutexLocker lk(&m_syncDataMutex);
            SyncDataMap::iterator syncDataIt = m_syncData.find(chunkKey);
            if (syncDataIt == m_syncData.cend()) {
                m_syncData.emplace(
                    chunkKey,
                    SyncData(
                        0.0,
                        fromCatalog->findFileIndex(
                            chunkKey.chunk.startTimeMs, 
                            DeviceFileCatalog::FindMethod::OnRecordHole_NextChunk
                        )
                    )
                );
            } else {
                auto catalogSize = fromCatalog->size();
                int curFileIndex = fromCatalog->findFileIndex(
                    chunkKey.chunk.startTimeMs, 
                    DeviceFileCatalog::FindMethod::OnRecordHole_NextChunk
                );
                syncDataIt->second.coeff = (double)(curFileIndex - syncDataIt->second.startIndex) /
                                           (double)((catalogSize - 1) - syncDataIt->second.startIndex);
            }
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

        if (!fromFile) {
            NX_LOG(lit("[Backup::copyFile] Source file %1 open failed")
                        .arg(fromFileFullName),
                   cl_logWARNING);
            return CopyError::SourceFileError;
        }

        auto optimalRootBackupPred = [](const QnStorageResourcePtr & storage) {
            return storage->getFreeSpace() > storage->getSpaceLimit();
        };
        auto relativeFileName = fromFileFullName.mid(fromStorage->getUrl().size());
        auto toStorage = qnBackupStorageMan->getOptimalStorageRoot(
            nullptr,
            optimalRootBackupPred
        );

        if (!toStorage) {
            qnBackupStorageMan->clearSpace(true);
            toStorage = qnBackupStorageMan->getOptimalStorageRoot(
                nullptr,
                optimalRootBackupPred
            );
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
            Q_ASSERT(bitrate > 0);
            qint64 fileSize = fromFile->size();
            const qint64 timeToWrite = (fileSize / bitrate) * 1000;
                            
            const qint64 CHUNK_SIZE = 4096;
            const qint64 chunksInFile = fileSize / CHUNK_SIZE;
            const qint64 timeOnChunk = timeToWrite / chunksInFile;

            while (fileSize > 0)
            {
                if (m_interrupted || !m_backupSyncOn) {
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

        if (m_interrupted || !m_backupSyncOn) {
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

template<typename NeedMoveOnCB>
QnServer::BackupResultCode QnScheduleSync::synchronize(NeedMoveOnCB needMoveOn)
{
    NX_LOG("[Backup] Starting...", cl_logDEBUG2);
    QnMutexLocker lock(&m_syncPointMutex);
    auto chunk = findLastSyncChunkUnsafe();
    m_syncTimePoint = chunk.startTimeMs;
    m_syncEndTimePoint = chunk.endTimeMs();

    while (1) {
        auto chunkKeyVector = getOldestChunk(m_syncTimePoint);
        if (!chunkKeyVector) {
            NX_LOG("[Backup] chunks ended, backup done", cl_logDEBUG2);
            break;
        }
        else {
            m_syncTimePoint = (*chunkKeyVector)[0].chunk.startTimeMs;
            m_syncEndTimePoint = qMax((*chunkKeyVector)[0].chunk.endTimeMs(), 
                                      m_syncEndTimePoint.load());
            NX_LOG(lit("[Backup] found chunk to backup: %1").arg(m_syncTimePoint),
                   cl_logDEBUG2);
        }
        for (const auto &chunkKey : *chunkKeyVector) {
            auto err = copyChunk(chunkKey);
            if (err != CopyError::NoError) {
                NX_LOG(
                    lit("[QnScheduleSync::synchronize] %1").arg(copyErrorString(err)),
                    cl_logWARNING
                );
                if (err == CopyError::NoBackupStorageError || 
                    err == CopyError::FromStorageError ||
                    err == CopyError::TargetFileError) {
                    return QnServer::BackupResultCode::Failed;
                } else if (err == CopyError::Interrupted) {
                    return QnServer::BackupResultCode::Cancelled;
                }
            }
        }
        auto needMoveOnCode = needMoveOn();
        bool needStop = needMoveOnCode != SyncCode::Ok;
        if (needStop) {
            if (needMoveOnCode == SyncCode::OutOfTime) {
                return QnServer::BackupResultCode::EndOfPeriod;
            } else if (needMoveOnCode == SyncCode::WrongBackupType || 
                       needMoveOnCode == SyncCode::Interrupted) {
                return QnServer::BackupResultCode::Cancelled;
            }
            break;
        }
    }
    m_interrupted = true; // we are done till the next backup period
    return QnServer::BackupResultCode::Done;
}

void QnScheduleSync::stop() 
{ 
    m_backupSyncOn  = false;
    m_forced        = false;
    m_syncing       = false;
    m_interrupted   = true;
   
    wait();
}

int QnScheduleSync::state() const 
{
    int result = 0;
    result |= m_backupSyncOn ? Started : Idle;
    result |= m_syncing ? Syncing : 0;
    result |= m_forced ? Forced : 0;
    return result;
}

int QnScheduleSync::interrupt()
{
    if (!m_backupSyncOn)
        return Idle;
    m_syncing = false;
    m_forced = false;
    m_interrupted = true;
    return Ok;
}

int QnScheduleSync::forceStart()
{
    if (!m_backupSyncOn)
        return Idle;
    if (m_forced)
        return Forced;
    
    m_forced = true;
    m_interrupted = false;
    return Ok;
}

QnBackupStatusData QnScheduleSync::getStatus() const
{
    QnBackupStatusData ret;
    ret.state = m_syncing || m_forced ? Qn::BackupState_InProgress :
                                        Qn::BackupState_None;
    ret.backupTimeMs = m_syncEndTimePoint;
    {
        QnMutexLocker lk(&m_syncDataMutex);
        auto syncDataSize = (double)m_syncData.size();
        ret.progress = std::accumulate(
            m_syncData.cbegin(),
            m_syncData.cend(),
            0.0,
            [](double ac, const SyncDataMap::value_type &p)
            {
                return ac + p.second.coeff;
            }
        ) / (syncDataSize ? syncDataSize : 1);
    }
    return ret;
}

void QnScheduleSync::renewSchedule()
{
    auto server = qnCommon->currentServer();
    Q_ASSERT(server);

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
    static const auto 
    REDUNDANT_SYNC_TIMEOUT = std::chrono::seconds(5);
    m_backupSyncOn = true;

    while (m_backupSyncOn)
    {
        std::this_thread::sleep_for(
            REDUNDANT_SYNC_TIMEOUT
        );

        if (!m_backupSyncOn.load())
            return;

        renewSchedule();

        auto isItTimeForSync = [this] ()
        {
            if (!m_backupSyncOn)
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
            m_syncing = true;
            auto result = synchronize(isItTimeForSync);
            {
                QnMutexLocker lk(&m_syncDataMutex);
                m_syncData.clear();
            }
            m_forced = false;
            m_syncing = false;

            if (result == QnServer::BackupResultCode::Failed && !m_failReported) {
                emit backupFinished(m_syncEndTimePoint, result);
                m_failReported = true;
            } else if (result != QnServer::BackupResultCode::Failed) {
                emit backupFinished(m_syncEndTimePoint, result);
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
    return key1.cameraID < key2.cameraID ?
           true : key1.cameraID > key2.cameraID ?
                  false : key1.catalog < key2.catalog;
}
