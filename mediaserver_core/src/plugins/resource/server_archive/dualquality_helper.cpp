#include "dualquality_helper.h"

#include <core/resource/network_resource.h>

#include <server/server_globals.h>

#include "recorder/storage_manager.h"
#include "core/resource/network_resource.h"
#include "utils/common/log.h"

#include <cmath>

static const int SECOND_STREAM_FIND_EPS = 1000 * 5;
static const int FIRST_STREAM_FIND_EPS = 1000 * 15;

QnDualQualityHelper::QnDualQualityHelper()
{
    m_quality = MEDIA_Quality_High;
    m_alreadyOnAltChunk = false;
}

void QnDualQualityHelper::setResource(const QnNetworkResourcePtr &netResource) {
    openCamera(netResource->getUniqueId());
}

void QnDualQualityHelper::openCamera(const QString& cameraUniqueId) {
    m_catalogHi[QnServer::StoragePool::Normal] = 
        qnNormalStorageMan->getFileCatalog(cameraUniqueId, 
                                           QnServer::HiQualityCatalog);

    m_catalogHi[QnServer::StoragePool::Backup] = 
        qnBackupStorageMan->getFileCatalog(cameraUniqueId, 
                                           QnServer::HiQualityCatalog);

    m_catalogLow[QnServer::StoragePool::Normal] = 
        qnNormalStorageMan->getFileCatalog(cameraUniqueId, 
                                           QnServer::LowQualityCatalog);

    m_catalogLow[QnServer::StoragePool::Backup] = 
        qnBackupStorageMan->getFileCatalog(cameraUniqueId, 
                                           QnServer::LowQualityCatalog);
}

void QnDualQualityHelper::findDataForTime(
    const qint64                        time,
    DeviceFileCatalog::TruncableChunk   &chunk,
    DeviceFileCatalogPtr                &catalog,
    DeviceFileCatalog::FindMethod       findMethod,
    bool                                preciseFind
)
{
    DeviceFileCatalogPtr normalCatalog = (m_quality == MEDIA_Quality_Low ? 
                                          m_catalogLow[QnServer::StoragePool::Normal] : 
                                          m_catalogHi[QnServer::StoragePool::Normal]);

    DeviceFileCatalogPtr normalCatalogAlt = (m_quality == MEDIA_Quality_Low ? 
                                          m_catalogHi[QnServer::StoragePool::Normal] : 
                                          m_catalogLow[QnServer::StoragePool::Normal]);

    DeviceFileCatalogPtr backupCatalog = (m_quality == MEDIA_Quality_Low ? 
                                          m_catalogLow[QnServer::StoragePool::Backup] : 
                                          m_catalogHi[QnServer::StoragePool::Backup]);

    DeviceFileCatalogPtr backupCatalogAlt = (m_quality == MEDIA_Quality_Low ? 
                                          m_catalogHi[QnServer::StoragePool::Backup] : 
                                          m_catalogLow[QnServer::StoragePool::Backup]);

    SearchStack searchStack;

    searchStack.emplace(backupCatalogAlt);
    searchStack.emplace(normalCatalogAlt);
    searchStack.emplace(backupCatalog);
    searchStack.emplace(normalCatalog);

    bool usePreciseFind = preciseFind || m_alreadyOnAltChunk; 
    m_alreadyOnAltChunk = false;

    findDataForTimeHelper(
        time, 
        chunk, 
        catalog, 
        findMethod, 
        usePreciseFind,
        searchStack,
        -1 // There is no previous distance yet
    );
}

void QnDualQualityHelper::findDataForTimeHelper(
    const qint64                        time,
    DeviceFileCatalog::TruncableChunk   &resultChunk,
    DeviceFileCatalogPtr                &resultCatalog,
    DeviceFileCatalog::FindMethod       findMethod,
    bool                                preciseFind,
    SearchStack                         &searchStack,
    qint64                              previousDistance
)
{
    if (searchStack.empty() || previousDistance == 0)
        return;
    
    DeviceFileCatalogPtr currentCatalog = searchStack.top();
    searchStack.pop();

    if (!currentCatalog)
        return findDataForTimeHelper(
            time, 
            resultChunk, 
            resultCatalog, 
            findMethod, 
            preciseFind,
            searchStack,
            previousDistance
        );

    DeviceFileCatalog::TruncableChunk currentChunk = currentCatalog->chunkAt(
        currentCatalog->findFileIndex(time, findMethod)
    );
    
    if (currentChunk.startTimeMs == -1)
        return findDataForTimeHelper(
            time, 
            resultChunk, 
            resultCatalog, 
            findMethod, 
            preciseFind,
            searchStack,
            previousDistance
        );
    
    qint64 currentDistance = calcDistanceHelper(currentChunk, time, findMethod);

    NX_LOG(lit("seek time: %4, current chunk: %1, current distance: %2, previous distance: %3, camera: %5")
                .arg(currentChunk.startTimeMs).arg(currentDistance).arg(previousDistance)
                .arg(time).arg(currentCatalog->cameraUniqueId()),
           cl_logDEBUG2);
    if (previousDistance == -1)
    {
        resultChunk   = currentChunk;
        resultCatalog = currentCatalog;

        return findDataForTimeHelper(
            time, 
            resultChunk, 
            resultCatalog, 
            findMethod, 
            preciseFind,
            searchStack,
            currentDistance
        );
    }

    int findEps = 0;

    if (!preciseFind)
    {
        QnServer::ChunksCatalog currentQuality  = currentCatalog->getRole();
        QnServer::ChunksCatalog previousQuality = resultCatalog->getRole();

        if (currentQuality == QnServer::LowQualityCatalog)
            findEps = SECOND_STREAM_FIND_EPS;
        else
            findEps = FIRST_STREAM_FIND_EPS;
    }

    if (previousDistance > currentDistance + findEps)
    {
        if (currentChunk.containsTime(resultChunk.startTimeMs) && previousDistance != INT64_MAX)
            currentChunk.truncate(resultChunk.startTimeMs);

        resultChunk = currentChunk;
        resultCatalog = currentCatalog;
        m_alreadyOnAltChunk = true;

        return findDataForTimeHelper(
            time, 
            resultChunk, 
            resultCatalog, 
            findMethod, 
            preciseFind,
            searchStack,
            currentDistance
        );
    }
    else
    {
        return findDataForTimeHelper(
            time, 
            resultChunk, 
            resultCatalog, 
            findMethod, 
            preciseFind,
            searchStack,
            previousDistance
        );
    }
}

int64_t QnDualQualityHelper::calcDistanceHelper(
    const DeviceFileCatalog::Chunk  &chunk,
    const int64_t                   time,
    DeviceFileCatalog::FindMethod   findMethod
)
{
    qint64 timeDistance = chunk.distanceToTime(time);
    if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && 
        chunk.endTimeMs() <= time)
    {
        timeDistance = INT64_MAX; // actually chunk not found
    }
    return timeDistance;
}

void QnDualQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    if (m_quality != quality) {
        m_quality = quality;
        m_alreadyOnAltChunk = false;
    }
}
