#include <utils/common/log.h>
#include <utils/common/synctime.h>

#include <recorder/storage_manager.h>
#include <nx_ec/data/api_media_server_data.h>
#include <core/resource_management/resource_pool.h>
#include "common/common_module.h"
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
      m_forced(false)
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

template<typename NeedStopCB>
void QnScheduleSync::synchronize(
    QnServer::ChunksCatalog catalog,
    const QString           &cameraId,
    NeedStopCB              needStop
)
{
    auto fromCatalog = qnNormalStorageMan->getFileCatalog(
        cameraId,
        catalog
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
        cameraId,
        catalog
    );
    if (!toCatalog)
    {
        NX_LOG(
            lit("[QnScheduleSync::synchronize] get toCatalog failed"),
            cl_logDEBUG1
        );
        return;
    }

    DeviceFileCatalog::Chunk chunk;
    if (toCatalog->isEmpty())
        chunk = fromCatalog->chunkAt(
            fromCatalog->findFileIndex(
                fromCatalog->firstTime(),
                DeviceFileCatalog::OnRecordHole_PrevChunk
            )
        );
    else
        chunk = fromCatalog->chunkAt(
            fromCatalog->findFileIndex(
                toCatalog->lastChunkStartTime(),
                DeviceFileCatalog::OnRecordHole_NextChunk
            )
        );

    if (chunk.startTimeMs == -1) // not found
    {
        NX_LOG(
            lit("[QnScheduleSync::synchronize] first chunk not found"),
            cl_logDEBUG1
        );
        return;
    }

    while (true)
    {
        // copy file
        QString fromFileFullName = fromCatalog->fullFileName(chunk);            
        auto fromStorage = qnNormalStorageMan->getStorageByUrl(fromFileFullName);

        std::unique_ptr<QIODevice> fromFile = std::unique_ptr<QIODevice>(
            fromStorage->open(
                fromFileFullName,
                QIODevice::ReadOnly
            )
        );
        auto relativeFileName = fromFileFullName.mid(fromStorage->getUrl().size());
        auto toStorage = qnBackupStorageMan->getOptimalStorageRoot(nullptr);
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
                cl_logDEBUG1
            );
            continue;
        }

        int bitrate = m_schedule.bitrate;
        if (bitrate == -1 || bitrate == 0) // not capped
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

        // add chunk
        // TODO: #backup storages #akulikov Deal with null provider
        qnBackupStorageMan->fileStarted(
            chunk.startTimeMs,
            chunk.timeZone,
            newFileName,
            nullptr
        );

        qnBackupStorageMan->fileFinished(
            chunk.durationMs,
            newFileName,
            nullptr,
            chunk.getFileSize()
        );

        // get next chunk
        chunk = fromCatalog->chunkAt(
            fromCatalog->findFileIndex(
                chunk.startTimeMs,
                DeviceFileCatalog::OnRecordHole_NextChunk
            )
        );

        if (chunk.startTimeMs == -1 || fromCatalog->isLastChunk(chunk.startTimeMs) || needStop())
            break;
    }
}

template<typename NeedStopCB>
void QnScheduleSync::synchronize(NeedStopCB needStop)
{
    //typedef std::deque<DeviceFileCatalog::Chunk>    ChunkContainerType;
    //typedef std::list<DeviceFileCatalog::Chunk>     ChunkListType;
    //typedef ChunkListType::const_iterator           ChunkConstIteratorType;
    //typedef ChunkListType::reverse_iterator         ChunkReverseIteratorType;
    //typedef std::vector<ChunkReverseIteratorType>   CRIteratorVectorType;
    //typedef std::vector<ChunkConstIteratorType>     CCIteratorVectorType;

    //const int CATALOG_QUALITY_COUNT = 2;


    auto resource = qnResPool->getResourceById(
        qnCommon->moduleGUID()
    );
    auto mediaServer = resource.dynamicCast<QnMediaServerResource>();
    Q_ASSERT(mediaServer);
    
    auto allCameras = qnResPool->getAllCameras(mediaServer);
    std::vector<QString> visitedCameras(allCameras.size());
    
    for (auto &camera : allCameras)
    {
        auto visitedIt = std::find_if(
            visitedCameras.cbegin(),
            visitedCameras.cend(),
            [&camera](const QString &cameraId)
            {
                return camera->getUniqueId() == cameraId;
            }
        );
        if (visitedIt != visitedCameras.cend())
            break;
        auto securityCamera = camera.dynamicCast<QnSecurityCamResource>();
        Q_ASSERT(securityCamera);
        visitedCameras.push_back(camera->getUniqueId());
        auto backupType = securityCamera->getBackupType();
        if (backupType == Qn::CameraBackup_Disabled)
            break;
        else if (backupType & Qn::CameraBackup_HighQuality)
            synchronize(QnServer::HiQualityCatalog, camera->getUniqueId(), needStop);
        else if (backupType & Qn::CameraBackup_LowQuality)
            synchronize(QnServer::LowQualityCatalog, camera->getUniqueId(), needStop);
    }

    //for (int qualityIndex = 0; 
    //     qualityIndex < CATALOG_QUALITY_COUNT; 
    //     ++qualityIndex)
    //{
    //    std::vector<QString>    visitedCameras;
    //    ChunkListType           currentChunks;
    //    QString                 currentCamera;

    //    auto getUnvisited = 
    //        [this, &visitedCameras, qualityIndex]
    //        (QString &currentCamera)
    //        {
    //            QnMutexLocker lk(&m_mutexCatalog);
    //            const FileCatalogMap &currentMap = m_devFileCatalog[qualityIndex];
    //            
    //            for (auto it = currentMap.cbegin();
    //                 it != currentMap.cend();
    //                 ++it)
    //            {
    //                const QString &cameraID = it.key();
    //                auto visitedIt = std::find(
    //                    visitedCameras.cbegin(),
    //                    visitedCameras.cend(),
    //                    cameraID
    //                );
    //                if (visitedIt == visitedCameras.cend())
    //                {
    //                    if (it.value()->m_chunks.size() <= 1)
    //                        continue;
    //                    currentCatalog.assign(
    //                        it.value()->m_chunks.cbegin(), 
    //                        --it.value()->m_chunks.cend() // get all chunks but last (it may be being recorded just now)
    //                    );
    //                    currentCamera = cameraID;
    //                    visitedCameras.push_back(cameraID);
    //                    return true;
    //                }
    //            }
    //            return false;
    //        };

    //    auto mergeNewChunks = 
    //        [this, qualityIndex](const QString &cameraID, const ChunkListType &newChunks)
    //        {
    //            QnMutexLocker lk(&m_mutexCatalog);
    //            FileCatalogMap currentMap = m_devFileCatalog[qualityIndex];

    //            auto cameraCatalogIt = currentMap.find(cameraID);
    //            if (cameraCatalogIt == currentMap.end())
    //                return;

    //            ChunkContainerType &catalogChunks = cameraCatalogIt.value()->m_chunks;
    //            auto startCatalogIt = catalogChunks.begin();

    //            ChunkContainerType mergedChunks;
    //            auto it = newChunks.cbegin();

    //            while (it != newChunks.cend())
    //            {
    //                auto chunkIt = std::find_if(
    //                    startCatalogIt,
    //                    catalogChunks.end(),
    //                    [&it](const DeviceFileCatalog::Chunk &chunk)
    //                    {
    //                        return chunk.startTimeMs == it->startTimeMs;
    //                    }
    //                );

    //                if (chunkIt == catalogChunks.end())
    //                {
    //                    mergedChunks.insert(
    //                        mergedChunks.end(),
    //                        startCatalogIt, 
    //                        chunkIt
    //                    );
    //                    break;
    //                }

    //                auto startTimeMs = chunkIt->startTimeMs;
    //                auto newChunksRange = aux::equalRangeBy_startTimeMs(
    //                    it, 
    //                    newChunks.end(), 
    //                    startTimeMs
    //                );
    //                auto catalogChunksRange = aux::equalRangeBy_startTimeMs(
    //                    chunkIt, 
    //                    catalogChunks.end(), 
    //                    startTimeMs
    //                );

    //                mergedChunks.insert(
    //                    mergedChunks.end(),
    //                    catalogChunksRange.first, 
    //                    catalogChunksRange.second
    //                );

    //                while (newChunksRange.first != newChunksRange.second)
    //                {
    //                    auto newChunkExistIt = std::find_if(
    //                        catalogChunksRange.first, 
    //                        catalogChunksRange.second,
    //                        [&newChunksRange](const DeviceFileCatalog::Chunk &chunk)
    //                        {
    //                            return newChunksRange.first->storageIndex == chunk.storageIndex;
    //                        }
    //                    );

    //                    if (newChunkExistIt == catalogChunksRange.second)
    //                        mergedChunks.insert(mergedChunks.end(), *newChunksRange.first);
    //                    ++newChunksRange.first;
    //                }
    //                
    //                it = newChunksRange.second;
    //                startCatalogIt = catalogChunksRange.second;
    //            }
    //            mergedChunks.push_back(catalogChunks.back());
    //            catalogChunks = std::move(mergedChunks);
    //        };

    //    auto copyChunk =
    //        [&storage, needStop, &currentChunks, this, qualityIndex, &mergeNewChunks] (
    //            const CCIteratorVectorType    &currentChunkRange,
    //            const QString                 &currentCamera
    //        )
    //        {
    //            int currentStorageIndex = getStorageIndex(storage);
    //            auto chunkOnStorageIt = std::find_if(
    //                currentChunkRange.cbegin(),
    //                currentChunkRange.cend(),
    //                [currentStorageIndex](const ChunkConstIteratorType& c)
    //                {
    //                    DeviceFileCatalog::Chunk chunk = *c;
    //                    return chunk.storageIndex == currentStorageIndex;
    //                }
    //            );
    //            if (chunkOnStorageIt == currentChunkRange.cend())
    //            {
    //                // copy from storage to storage
    //                auto newChunk = *(*currentChunkRange.begin());
    //                auto fromStorage = storageRoot(newChunk.storageIndex);

    //                {
    //                    QString fromFileFullName;
    //                    {
    //                        QnMutexLocker lk(&m_mutexCatalog);
    //                        fromFileFullName = m_devFileCatalog[qualityIndex][currentCamera]->fullFileName(newChunk);
    //                    }


    //                    std::unique_ptr<QIODevice> fromFile = std::unique_ptr<QIODevice>(
    //                        fromStorage->open(
    //                            fromFileFullName,
    //                            QIODevice::ReadOnly
    //                        )
    //                    );
    //                    
    //                    auto relativeFileName = fromFileFullName.mid(fromStorage->getUrl().size());

    //                    std::unique_ptr<QIODevice> toFile = std::unique_ptr<QIODevice>(
    //                        storage->open(
    //                            storage->getUrl() + relativeFileName,
    //                            QIODevice::WriteOnly | QIODevice::Append
    //                        )
    //                    );

    //                    if (!fromFile || !toFile)
    //                        return true;

    //                    int bitrate = storage->getRedundantSchedule().bitrate;
    //                    if (bitrate == -1 || bitrate == 0) // not capped
    //                    {
    //                        auto data = fromFile->readAll();
    //                        toFile->write(data);
    //                    }
    //                    else
    //                    {
    //                        Q_ASSERT(bitrate > 0);
    //                        qint64 fileSize = fromFile->size();
    //                        const qint64 timeToWrite = (fileSize / bitrate) * 1000;
    //                        
    //                        const qint64 CHUNK_SIZE = 4096;
    //                        const qint64 chunksInFile = fileSize / CHUNK_SIZE;
    //                        const qint64 timeOnChunk = timeToWrite / chunksInFile;

    //                        while (fileSize > 0)
    //                        {
    //                            qint64 startTime = qnSyncTime->currentMSecsSinceEpoch();
    //                            const qint64 writeSize = CHUNK_SIZE < fileSize ? CHUNK_SIZE : fileSize;
    //                            auto data = fromFile->read(writeSize);
    //                            if (toFile->write(data) == -1)
    //                                return true;
    //                            fileSize -= writeSize;
    //                            qint64 now = qnSyncTime->currentMSecsSinceEpoch();
    //                            if (now - startTime < timeOnChunk)
    //                                std::this_thread::sleep_for(
    //                                    std::chrono::milliseconds(timeOnChunk - (now - startTime))
    //                                );
    //                        }
    //                    }
    //                }

    //                newChunk.storageIndex = currentStorageIndex;
    //                auto forwardIt = *currentChunkRange.cbegin();
    //                ++forwardIt;

    //                currentChunks.insert(
    //                    forwardIt,
    //                    newChunk
    //                );
 
    //                if (needStop())
    //                {
    //                    mergeNewChunks(currentCamera, currentChunks);
    //                    return false;
    //                }
    //            }
    //            return true;
    //        };

    //    while (getUnvisited(currentCamera, currentChunks))
    //    {
    //        if (currentChunks.empty())
    //            continue;

    //        CCIteratorVectorType currentChunkRange;
    //        auto reverseChunkIt = currentChunks.cend();
    //        --reverseChunkIt;

    //        while (true)
    //        {
    //            auto currentChunk = *reverseChunkIt;

    //            if (currentChunkRange.empty() || 
    //                (*currentChunkRange.cbegin())->startTimeMs == currentChunk.startTimeMs)
    //            {
    //                currentChunkRange.push_back(reverseChunkIt);
    //                if (reverseChunkIt != currentChunks.cbegin())
    //                    --reverseChunkIt;
    //                else
    //                {
    //                    copyChunk(currentChunkRange, currentCamera);
    //                    break;
    //                }
    //            }                
    //            else if (!currentChunkRange.empty())
    //            {
    //                if (!copyChunk(currentChunkRange, currentCamera))
    //                    return;
    //                currentChunkRange.clear();
    //            }
    //        } // for reverse chunks iterator
    //        mergeNewChunks(currentCamera, currentChunks);
    //    } // while getUnvisited() == true
    //} // for quality index
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
