#include <utils/common/log.h>
#include <utils/common/synctime.h>

#include <recorder/storage_manager.h>
#include <nx_ec/data/api_media_server_data.h>
#include <core/resource_management/resource_pool.h>
#include "common/common_module.h"
#include <utils/common/util.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_resource.h>

#include "schedule_sync.h"

#include <mutex>

namespace aux
{
    template<typename Cont>
    typename Cont::const_iterator 
    constIteratorFromReverse(typename Cont::reverse_iterator it)
    {
        return (it+1).base();
    }

#define DEFINE_EQUAL_RANGE_BY_MEMBER(memberName) \
    template< \
        typename ForwardIt, \
        typename T, \
        typename CheckMember = decltype( \
                     std::declval<typename std::iterator_traits<ForwardIt>::value_type>().memberName, void() \
                 ), \
        typename CheckMemberType = typename std::enable_if< \
                                        std::is_same< \
                                            typename std::remove_reference<T>::type, \
                                            typename std::remove_reference< \
                                                decltype(std::declval<typename std::iterator_traits<ForwardIt>::value_type>().memberName) \
                                            >::type \
                                        >::value>::type \
    > \
    struct equalRangeByHelper \
    { \
        std::pair<ForwardIt, ForwardIt> operator ()(ForwardIt begin , ForwardIt end, T&& value) \
        { \
            std::pair<ForwardIt, ForwardIt> result = std::make_pair(end, end); \
            while (begin != end) \
            { \
                if (result.first == end && begin->memberName == value) \
                    result.first = begin; \
                if (result.first != end && begin->memberName != value) \
                { \
                    result.second = begin; \
                    break; \
                } \
                ++begin; \
            } \
            return result; \
        } \
    }; \
     \
    template<typename ForwardIt, typename T> \
    std::pair<ForwardIt, ForwardIt> equalRangeBy_##memberName(ForwardIt begin , ForwardIt end, T&& value) \
    { \
        return equalRangeByHelper<ForwardIt, T>()(begin, end, std::forward<T>(value)); \
    }

    DEFINE_EQUAL_RANGE_BY_MEMBER(startTimeMs)
}

static std::once_flag QnScheduleSync_flag;
static QnScheduleSync *QnScheduleSync_instance = nullptr;

QnScheduleSync *QnScheduleSync::instance()
{
    return QnScheduleSync_instance;
}

QnScheduleSync::QnScheduleSync()
    : m_backupSyncOn(false),
      m_syncing(false),
      m_forced(false),
      m_syncTimePoint(0)
{
    std::call_once(
        QnScheduleSync_flag, 
        [this]
        {
            QnScheduleSync_instance = this;
        }
    );
    start();
}

QnScheduleSync::~QnScheduleSync()
{
    stop();
}

QnScheduleSync::ChunkKey QnScheduleSync::getOldestChunk(
    const QString           &cameraId,
    QnServer::ChunksCatalog catalog
)
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
    
    auto chunk = fromCatalog->chunkAt(
        fromCatalog->findNextFileIndex(m_syncTimePoint)
    );
    ChunkKey ret = {chunk, cameraId, catalog};
    return ret;
}


boost::optional<QnScheduleSync::ChunkKeyVector> 
QnScheduleSync::getOldestChunk() 
{
    ChunkKeyVector ret;
    auto mediaServer = 
        qnResPool->getResourceById(qnCommon->moduleGUID())
            .dynamicCast<QnMediaServerResource>();
    
    Q_ASSERT(mediaServer);    
    int64_t minTime = std::numeric_limits<int64_t>::max();

    for (auto &camera : qnResPool->getAllCameras(mediaServer)) 
    {
        auto securityCamera = camera.dynamicCast<QnSecurityCamResource>();
        Q_ASSERT(securityCamera);
        
        auto backupType = securityCamera->getActualBackupType();
        ChunkKey tmp;

        if (backupType & Qn::CameraBackup_HighQuality)
            tmp = getOldestChunk(camera->getUniqueId(), QnServer::HiQualityCatalog);
        if (tmp.chunk.startTimeMs != -1 && tmp.chunk.startTimeMs < minTime)
        {
            minTime = tmp.chunk.startTimeMs;
            ret.clear();
            ret.push_back(tmp);
        }
        else if (tmp.chunk.startTimeMs == minTime)
        {
            ret.push_back(tmp);
        }

        if (backupType & Qn::CameraBackup_LowQuality)
            tmp = getOldestChunk(camera->getUniqueId(), QnServer::LowQualityCatalog);
        if (tmp.chunk.startTimeMs != -1 && tmp.chunk.startTimeMs < minTime)
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
    
    if (!ret.empty())
        m_syncTimePoint = ret[0].chunk.startTimeMs;
    else
        return boost::none;

    return ret;
}

void QnScheduleSync::copyChunk(const ChunkKey &chunkKey)
{
    auto fromCatalog = qnNormalStorageMan->getFileCatalog(
        chunkKey.cameraID,
        chunkKey.catalog
    );
    if (!fromCatalog)
    {
        NX_LOG(
            lit("[QnScheduleSync::synchronize] get fromCatalog failed"),
            cl_logDEBUG1
        );
        return;
    }

    auto toCatalog = qnBackupStorageMan->getFileCatalog(
        chunkKey.cameraID,
        chunkKey.catalog
    );
    if (!toCatalog)
    {
        NX_LOG(
            lit("[QnScheduleSync::synchronize] get toCatalog failed"),
            cl_logDEBUG1
        );
        return;
    }

    if (toCatalog->getLastSyncTime() < chunkKey.chunk.startTimeMs)
    {
        QString fromFileFullName = fromCatalog->fullFileName(chunkKey.chunk);            
        auto fromStorage = qnNormalStorageMan->getStorageByUrl(fromFileFullName);

        std::unique_ptr<QIODevice> fromFile = std::unique_ptr<QIODevice>(
            fromStorage->open(
                fromFileFullName,
                QIODevice::ReadOnly
            )
        );
        auto relativeFileName = fromFileFullName.mid(fromStorage->getUrl().size());
        auto toStorage = qnBackupStorageMan->getOptimalStorageRoot(nullptr);
        auto newStorageUrl = toStorage->getUrl();
        auto oldSeparator = getPathSeparator(relativeFileName);
        auto newSeparator = getPathSeparator(newStorageUrl);
        if (oldSeparator != newSeparator)
            relativeFileName.replace(oldSeparator, newSeparator);
        auto newFileName = toStorage->getUrl() + relativeFileName;

        std::unique_ptr<QIODevice> toFile = std::unique_ptr<QIODevice>(
            toStorage->open(
                newFileName,
                QIODevice::WriteOnly | QIODevice::Append
            )
        );

        if (!fromFile || !toFile)
        {
            NX_LOG(
                lit("[QnScheduleSync::synchronize] file %1 or %2 open error")
                    .arg(fromFileFullName)
                    .arg(newFileName),
                cl_logWARNING
            );
        }

        int bitrate = m_schedule.bitrate;
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
                    continue;
                }
                fileSize -= writeSize;
                qint64 now = qnSyncTime->currentMSecsSinceEpoch();
                if (now - startTime < timeOnChunk)
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(timeOnChunk - (now - startTime))
                    );
            }
        }

        // add chunk to catalog
        bool result = qnBackupStorageMan->fileStarted(
            chunkKey.chunk.startTimeMs,
            chunkKey.chunk.timeZone,
            newFileName,
            nullptr
        );

        if (!result)
        {
            NX_LOG(
                lit("[QnScheduleSync::synchronize] fileStarted() for file %1 failed")
                    .arg(newFileName),
                cl_logWARNING
            );
        }

        result = qnBackupStorageMan->fileFinished(
            chunkKey.chunk.durationMs,
            newFileName,
            nullptr,
            chunkKey.chunk.getFileSize()
        );

        if (!result)
            NX_LOG(
                lit("[QnScheduleSync::synchronize] fileFinished() for file %1 failed")
                    .arg(newFileName),
                cl_logWARNING
            );
    }
}

template<typename NeedStopCB>
void QnScheduleSync::synchronize(NeedStopCB needStop)
{
    while (1)
    {
        auto chunkKeyVector = getOldestChunk();
        if (!chunkKeyVector)
            break;
        for (const auto &chunkKey : *chunkKeyVector)
            copyChunk(chunkKey);
        if (needStop())
            break;
    }
}

int QnScheduleSync::stop() 
{ 
    if (!m_backupSyncOn)
        return Idle;
    
    m_backupSyncOn  = false;
    m_forced        = false;
    m_syncing       = false;
   
    m_backupFuture.wait();
    return Ok;
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
    return Ok;
}

int QnScheduleSync::forceStart()
{
    if (!m_backupSyncOn)
        return Idle;
    if (m_forced)
        return Forced;
    
    m_forced = true;
    return Ok;
}

void QnScheduleSync::renewSchedule()
{
    auto resource = qnResPool->getResourceById(
        qnCommon->moduleGUID()
    );
    auto mediaServer = resource.dynamicCast<QnMediaServerResource>();
    Q_ASSERT(mediaServer);

    if (mediaServer)
    {
        m_schedule.type     = mediaServer->getBackupType();
        m_schedule.dow      = mediaServer->getBackupDOW();
        m_schedule.start    = mediaServer->getBackupStart();
        m_schedule.duration = mediaServer->getBackupDuration();
        m_schedule.bitrate  = mediaServer->getBackupBitrate();
    }
}

void QnScheduleSync::start()
{
    static const auto 
    REDUNDANT_SYNC_TIMEOUT = std::chrono::seconds(10);
    m_backupSyncOn         = true;

    m_backupFuture = std::async(
        std::launch::async,
        [this]
        {
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
                    if (m_schedule.type != Qn::Backup_Schedule)
                        return false;
                    if (m_forced)
                        return true;

                    QDateTime now = QDateTime::fromMSecsSinceEpoch(qnSyncTime->currentMSecsSinceEpoch());                        
                    const auto curDate = now.date();

                    if ((ec2::backup::fromQtDOW(curDate.dayOfWeek()) & m_schedule.dow))
                    {
                        const auto curTime = now.time();
                        if (curTime.msecsSinceStartOfDay() > m_schedule.start * 1000 && 
                            m_schedule.duration == -1) // sync without end time
                        {
                            return true;
                        }

                        if (curTime.msecsSinceStartOfDay() > m_schedule.start * 1000 &&
                            curTime.msecsSinceStartOfDay() < m_schedule.start * 1000 + m_schedule.duration * 1000) // 'normal' schedule sync
                        {
                            return true;
                        }
                    }
                    return false;
                };

                if (isItTimeForSync()) // we are there
                {
                    m_syncing = true;
                    m_syncTimePoint = 0;

                    synchronize(
                        [this, isItTimeForSync]
                        {
                            if (!m_backupSyncOn || // stop forced
                                !isItTimeForSync() || // synchronization period is over
                                !m_syncing) // interrupted by user
                            {
                               return true;
                            }
                            return false;
                        }
                    );
                    m_forced  = false;
                    m_syncing = false;
                }
            }
        }
    );
}
