#include "dualquality_helper.h"

#include <core/resource/network_resource.h>

#include <server/server_globals.h>

#include "recorder/storage_manager.h"
#include "core/resource/network_resource.h"

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
    m_catalogHi[(int)QnServer::StoragePool::Normal] = 
        qnNormalStorageMan->getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);

    m_catalogHi[(int)QnServer::StoragePool::Backup] = 
        qnBackupStorageMan->getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);

    m_catalogLow[(int)QnServer::StoragePool::Normal] = 
        qnNormalStorageMan->getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);

    m_catalogLow[(int)QnServer::StoragePool::Backup] = 
        qnBackupStorageMan->getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
}

void QnDualQualityHelper::findDataForTimeHelper(
    const qint64                    time,
    DeviceFileCatalog::Chunk        &chunk,
    DeviceFileCatalogPtr            &catalog,
    DeviceFileCatalog::FindMethod   findMethod,
    bool                            preciseFind,
    QnServer::StoragePool           storageManager
)
{
    bool usePreciseFind = m_alreadyOnAltChunk || preciseFind;
    m_alreadyOnAltChunk = false;
    int index = static_cast<int>(storageManager);

    catalog = (m_quality == MEDIA_Quality_Low ? m_catalogLow[index] : m_catalogHi[index]);
    if (catalog == 0)
        return; // no data in archive

    chunk = catalog->chunkAt(catalog->findFileIndex(time, findMethod));

    qint64 timeDistance = chunk.distanceToTime(time);

    if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && chunk.endTimeMs() <= time)
        timeDistance = INT64_MAX; // actually chunk not found
    if (timeDistance > 0)
    {
        // chunk not found. check in alternate quality
        DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_Low ? m_catalogHi[index] : m_catalogLow[index]);
        DeviceFileCatalog::Chunk altChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(time, findMethod));
        qint64 timeDistanceAlt = altChunk.distanceToTime(time);
        if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs() <= time)
            timeDistanceAlt = INT64_MAX; // actually chunk not found

        int findEps = m_quality == MEDIA_Quality_Low ? FIRST_STREAM_FIND_EPS : SECOND_STREAM_FIND_EPS;
        if (timeDistance - timeDistanceAlt > findEps || (timeDistance > timeDistanceAlt && usePreciseFind))
        {
            if (timeDistanceAlt == 0)
            {
                // if recording hole, alt quality chunk may be slightly longer then primary. So, distance to such chunk still 0
                // prevent change quality, if such chunk rest very low
                if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs()-time < findEps)
                {
                    DeviceFileCatalog::Chunk nextAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.endTimeMs(), findMethod));
                    //if (nextAltChunk.startTimeMs > chunk.startTimeMs - findEps && chunk.startTimeMs > time)
                    if (nextAltChunk.startTimeMs > altChunk.endTimeMs() + findEps && chunk.startTimeMs > time && !usePreciseFind)
                        return; // It is data hole, but altChunk cover very right edge before hole. Ignore it.
                    if (nextAltChunk.startTimeMs == altChunk.startTimeMs)
                        return; // EOF reached
                }
                else if (findMethod == DeviceFileCatalog::OnRecordHole_PrevChunk && time - altChunk.startTimeMs < findEps)
                {
                    DeviceFileCatalog::Chunk prevAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.startTimeMs-1, findMethod));
                    if (prevAltChunk.endTimeMs() < chunk.endTimeMs() + findEps && chunk.endTimeMs() < time)
                        return;
                }
            }

            // alternate quality matched better
            if (timeDistance != INT64_MAX && altChunk.containsTime(chunk.startTimeMs))
                altChunk.truncate(chunk.startTimeMs); // truncate to start of the next chunk of the required quality (if next chunk of requested quality is exists)

            catalog = catalogAlt;
            chunk = altChunk;
            m_alreadyOnAltChunk = true;
        }
    }
}

void QnDualQualityHelper::findDataForTime(
    const qint64                    time, 
    DeviceFileCatalog::Chunk&       chunk, 
    DeviceFileCatalogPtr            &catalog, 
    DeviceFileCatalog::FindMethod   findMethod, 
    bool                            preciseFind
)
{
    DeviceFileCatalog::Chunk normalChunk;
    DeviceFileCatalog::Chunk backupChunk;
    DeviceFileCatalogPtr normalCatalog;
    DeviceFileCatalogPtr backupCatalog;

    findDataForTimeHelper(
        time,
        normalChunk,
        normalCatalog,
        findMethod,
        preciseFind,
        QnServer::StoragePool::Normal
    );

    findDataForTimeHelper(
        time,
        backupChunk,
        backupCatalog,
        findMethod,
        preciseFind,
        QnServer::StoragePool::Backup
    );

    bool normalBetter = std::abs(normalChunk.startTimeMs - time) <= 
                        std::abs(backupChunk.startTimeMs - time);
    if (normalBetter) {
        chunk = normalChunk;
        catalog = normalCatalog;
    }
    else {
        chunk = backupChunk;
        catalog = backupCatalog;
    }
}

void QnDualQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    if (m_quality != quality) {
        m_quality = quality;
        m_alreadyOnAltChunk = false;
    }
}
