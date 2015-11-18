#include "dualquality_helper.h"

#include <core/resource/network_resource.h>

#include <server/server_globals.h>

#include "recorder/storage_manager.h"
#include "core/resource/network_resource.h"

#include <cmath>

#define uDebug() if (0) (void)(0); else qDebug()

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
    bool usePreciseFind = m_alreadyOnAltChunk || preciseFind;
    m_alreadyOnAltChunk = false;

    DeviceFileCatalogPtr normalCatalog = (m_quality == MEDIA_Quality_Low ? 
                                          m_catalogLow[QnServer::StoragePool::Normal] : 
                                          m_catalogHi[QnServer::StoragePool::Normal]);

    DeviceFileCatalogPtr backupCatalog = (m_quality == MEDIA_Quality_Low ? 
                                          m_catalogLow[QnServer::StoragePool::Backup] : 
                                          m_catalogHi[QnServer::StoragePool::Backup]);
    if (normalCatalog == 0 && backupCatalog == 0)
        return; // no data in archive
    
    DeviceFileCatalog::TruncableChunk normalChunk; 
    if (normalCatalog)
        normalChunk = normalCatalog->chunkAt(normalCatalog->findFileIndex(time, 
                                                                          findMethod));
    DeviceFileCatalog::TruncableChunk backupChunk; 
    if (backupCatalog)
        backupChunk = backupCatalog->chunkAt(backupCatalog->findFileIndex(time, 
                                                                          findMethod));
    if (normalChunk.startTimeMs == -1 && backupChunk.startTimeMs == -1)
        return;

    bool normalBetter = normalChunk.startTimeMs != -1 && (backupChunk.startTimeMs == -1 ||
            calcDistanceHelper(normalChunk, time, findMethod) <= 
            calcDistanceHelper(backupChunk, time, findMethod));

    if (normalBetter) {
        chunk = normalChunk;
        catalog = normalCatalog;
    }
    else {
        chunk = backupChunk;
        catalog = backupCatalog;
    }

    qint64 timeDistance = calcDistanceHelper(chunk, time, findMethod);
    if (timeDistance > 0) {
        DeviceFileCatalogPtr normalCatalogAlt = (m_quality == MEDIA_Quality_Low ? 
                                              m_catalogHi[QnServer::StoragePool::Normal] : 
                                              m_catalogLow[QnServer::StoragePool::Normal]);

        DeviceFileCatalogPtr backupCatalogAlt = (m_quality == MEDIA_Quality_Low ? 
                                              m_catalogHi[QnServer::StoragePool::Backup] : 
                                              m_catalogLow[QnServer::StoragePool::Backup]);
        if (normalCatalogAlt == 0 && backupCatalogAlt == 0)
            return; 
        
        DeviceFileCatalog::TruncableChunk normalChunkAlt; 
        if (normalCatalogAlt)
            normalChunkAlt = normalCatalogAlt->chunkAt(
                    normalCatalogAlt->findFileIndex(time, findMethod));

        DeviceFileCatalog::TruncableChunk backupChunkAlt; 
        if (backupCatalogAlt)
            backupChunkAlt = backupCatalogAlt->chunkAt(
                    backupCatalogAlt->findFileIndex(time, findMethod));

        if (normalChunkAlt.startTimeMs == -1 && backupChunkAlt.startTimeMs == -1)
            return;

        bool normalBetterAlt = normalChunkAlt.startTimeMs != -1 && 
             (backupChunkAlt.startTimeMs == -1 ||
                calcDistanceHelper(normalChunkAlt, time, findMethod) <= 
                calcDistanceHelper(backupChunkAlt, time, findMethod));

        DeviceFileCatalog::TruncableChunk chunkAlt;
        DeviceFileCatalogPtr catalogAlt;

        if (normalBetterAlt) {
            catalogAlt = normalCatalogAlt;
            chunkAlt = normalChunkAlt;
        }
        else {
            catalogAlt = backupCatalogAlt;
            chunkAlt = backupChunkAlt;
        }

        qint64 timeDistanceAlt = calcDistanceHelper(chunkAlt, time, findMethod);
        int findEps = m_quality == MEDIA_Quality_Low ? FIRST_STREAM_FIND_EPS : 
                                                       SECOND_STREAM_FIND_EPS;

        if (timeDistance - timeDistanceAlt > findEps || 
            (timeDistance > timeDistanceAlt && usePreciseFind)) 
        {
            if (timeDistanceAlt == 0) 
            {
                // if recording hole, alt quality chunk may be slightly longer then primary. 
                // So, distance to such chunk still 0.
                // prevent change quality, if such chunk rest very low
                if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && 
                    chunkAlt.endTimeMs()-time < findEps) 
                {
                    DeviceFileCatalog::Chunk nextNormalChunkAlt;
                    if (normalCatalogAlt)
                        nextNormalChunkAlt = normalCatalogAlt->chunkAt(
                                normalCatalogAlt->findFileIndex(chunkAlt.endTimeMs(), 
                                                                findMethod));
                    DeviceFileCatalog::Chunk nextBackupChunkAlt; 
                    nextBackupChunkAlt = backupCatalogAlt->chunkAt(
                                backupCatalogAlt->findFileIndex(chunkAlt.endTimeMs(), 
                                                                findMethod));

                    bool isNormalNextTooFar = nextNormalChunkAlt.startTimeMs != -1 &&
                        nextNormalChunkAlt.startTimeMs > chunkAlt.endTimeMs() + findEps && 
                        chunk.startTimeMs > time && !usePreciseFind;

                    bool isBackupNextTooFar = nextBackupChunkAlt.startTimeMs != -1 &&
                        nextBackupChunkAlt.startTimeMs > chunkAlt.endTimeMs() + findEps && 
                        chunk.startTimeMs > time && !usePreciseFind;

                    bool isGapToNextTooLarge = isNormalNextTooFar && isBackupNextTooFar;

                    if (isGapToNextTooLarge)
                        return; // It is data hole, but altChunk cover very right edge before hole. Ignore it.

                    bool altEOFReached = 
                        nextNormalChunkAlt.startTimeMs == chunkAlt.startTimeMs &&
                        nextBackupChunkAlt.startTimeMs == chunkAlt.startTimeMs;

                    if (altEOFReached)
                        return; // EOF reached
                }
                else if (findMethod == DeviceFileCatalog::OnRecordHole_PrevChunk && 
                         time - chunkAlt.startTimeMs < findEps)
                {
                    DeviceFileCatalog::Chunk prevNormalChunkAlt;
                    if (normalCatalogAlt)
                        prevNormalChunkAlt = normalCatalogAlt->chunkAt(
                                normalCatalogAlt->findFileIndex(chunkAlt.startTimeMs-1, 
                                                                findMethod));

                    DeviceFileCatalog::Chunk prevBackupChunkAlt;
                    if (backupCatalogAlt)
                        prevBackupChunkAlt = backupCatalogAlt->chunkAt(
                                backupCatalogAlt->findFileIndex(chunkAlt.startTimeMs-1, 
                                                                findMethod));
                    bool isNormalTooFar = prevNormalChunkAlt.startTimeMs != -1 &&
                         prevNormalChunkAlt.endTimeMs() < chunk.endTimeMs() + findEps &&
                         chunk.endTimeMs() < time;

                    bool isBackupTooFar = prevBackupChunkAlt.startTimeMs != -1 &&
                         prevBackupChunkAlt.endTimeMs() < chunk.endTimeMs() + findEps &&
                         chunk.endTimeMs() < time;

                    bool isPrevAltTooFar = isNormalTooFar && isBackupTooFar;

                    if (isPrevAltTooFar)
                        return;
                }
            }

            // alternate quality matched better
            if (timeDistance != INT64_MAX && chunkAlt.containsTime(chunk.startTimeMs)) {
                uDebug() << lit("Alt Chunk truncated at %1").arg(chunk.startTimeMs);
                chunkAlt.truncate(chunk.startTimeMs); // truncate to start of the next chunk of the required quality (if next chunk of requested quality is exists)
            }

            catalog = catalogAlt;
            chunk = chunkAlt;
            m_alreadyOnAltChunk = true;
        }
    }
}

//void QnDualQualityHelper::findDataForTimeHelper(
//    const qint64                        time,
//    DeviceFileCatalog::TruncableChunk   &chunk,
//    DeviceFileCatalogPtr                &catalog,
//    DeviceFileCatalog::FindMethod       findMethod,
//    bool                                preciseFind,
//    QnServer::StoragePool               storageManager
//)
//{
//    bool usePreciseFind = m_alreadyOnAltChunk || preciseFind;
//    m_alreadyOnAltChunk = false;
//
//    catalog = (m_quality == MEDIA_Quality_Low ? m_catalogLow[storageManager] : 
//                                                m_catalogHi[storageManager]);
//    if (catalog == 0)
//        return; // no data in archive
//
//    chunk = catalog->chunkAt(catalog->findFileIndex(time, findMethod));
//    chunk.isPreferredQuality = true;
//
//    qint64 timeDistance = calcDistanceHelper(chunk, time, findMethod);
//    if (timeDistance > 0)
//    {
//        // chunk not found. check in alternate quality
//        DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_Low ? m_catalogHi[storageManager] : m_catalogLow[storageManager]);
//        DeviceFileCatalog::TruncableChunk altChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(time, findMethod));
//        qint64 timeDistanceAlt = calcDistanceHelper(altChunk, time, findMethod);
//        int findEps = m_quality == MEDIA_Quality_Low ? FIRST_STREAM_FIND_EPS : SECOND_STREAM_FIND_EPS;
//
//        if (timeDistance - timeDistanceAlt > findEps || (timeDistance > timeDistanceAlt && usePreciseFind))
//        {
//            if (timeDistanceAlt == 0)
//            {
//                // if recording hole, alt quality chunk may be slightly longer then primary. So, distance to such chunk still 0
//                // prevent change quality, if such chunk rest very low
//                if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs()-time < findEps)
//                {
//                    DeviceFileCatalog::Chunk nextAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.endTimeMs(), findMethod));
//                    //if (nextAltChunk.startTimeMs > chunk.startTimeMs - findEps && chunk.startTimeMs > time)
//                    if (nextAltChunk.startTimeMs > altChunk.endTimeMs() + findEps && chunk.startTimeMs > time && !usePreciseFind)
//                        return; // It is data hole, but altChunk cover very right edge before hole. Ignore it.
//                    if (nextAltChunk.startTimeMs == altChunk.startTimeMs)
//                        return; // EOF reached
//                }
//                else if (findMethod == DeviceFileCatalog::OnRecordHole_PrevChunk && time - altChunk.startTimeMs < findEps)
//                {
//                    DeviceFileCatalog::Chunk prevAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.startTimeMs-1, findMethod));
//                    if (prevAltChunk.endTimeMs() < chunk.endTimeMs() + findEps && chunk.endTimeMs() < time)
//                        return;
//                }
//            }
//
//            // alternate quality matched better
//            if (timeDistance != INT64_MAX && altChunk.containsTime(chunk.startTimeMs)) {
//                uDebug() << lit("Alt Chunk truncated at %1").arg(chunk.startTimeMs);
//                altChunk.truncate(chunk.startTimeMs); // truncate to start of the next chunk of the required quality (if next chunk of requested quality is exists)
//            }
//
//            catalog = catalogAlt;
//            chunk = altChunk;
//            chunk.isPreferredQuality = false;
//            m_alreadyOnAltChunk = true;
//        }
//    }
//}
//

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

//
//void QnDualQualityHelper::findDataForTime(
//    const qint64                        time, 
//    DeviceFileCatalog::TruncableChunk   &chunk, 
//    DeviceFileCatalogPtr                &catalog, 
//    DeviceFileCatalog::FindMethod       findMethod, 
//    bool                                preciseFind
//)
//{
//    DeviceFileCatalog::TruncableChunk normalChunk;
//    DeviceFileCatalog::TruncableChunk backupChunk;
//    DeviceFileCatalogPtr normalCatalog;
//    DeviceFileCatalogPtr backupCatalog;
//
//    findDataForTimeHelper(
//        time,
//        normalChunk,
//        normalCatalog,
//        findMethod,
//        preciseFind,
//        QnServer::StoragePool::Normal
//    );
//
//    findDataForTimeHelper(
//        time,
//        backupChunk,
//        backupCatalog,
//        findMethod,
//        preciseFind,
//        QnServer::StoragePool::Backup
//    );
//
//    qint64 timeDistanceNormal = calcDistanceHelper(
//        normalChunk, 
//        time, 
//        findMethod
//    );    
//    qint64 timeDistanceBackup = calcDistanceHelper(
//        backupChunk,
//        time,
//        findMethod
//    );
//    
//    int findEps = (m_quality == MEDIA_Quality_Low) ? FIRST_STREAM_FIND_EPS : 
//                                                     SECOND_STREAM_FIND_EPS; 
//    
//    bool normalBetter = timeDistanceNormal <= timeDistanceBackup ||
//                        ((timeDistanceBackup == 0) && (timeDistanceNormal < findEps)
//                                                   && !preciseFind);
//    if (normalBetter) {
//        chunk = normalChunk;
//        catalog = normalCatalog;
//    }
//    else {
//        chunk = backupChunk;
//        catalog = backupCatalog;
//    }
//}

void QnDualQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    if (m_quality != quality) {
        m_quality = quality;
        m_alreadyOnAltChunk = false;
    }
}
