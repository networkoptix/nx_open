#include "dualquality_helper.h"

#include <core/resource/network_resource.h>

#include <server/server_globals.h>

#include "recorder/storage_manager.h"
#include "core/resource/network_resource.h"

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
    m_catalogHi[QnServer::ArchiveKind::Normal] = 
        qnNormalStorageMan->getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);

    m_catalogHi[QnServer::ArchiveKind::Backup] = 
        qnBackupStorageMan->getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);

    m_catalogLow[QnServer::ArchiveKind::Normal] = 
        qnNormalStorageMan->getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);

    m_catalogLow[QnServer::ArchiveKind::Backup] = 
        qnBackupStorageMan->getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
}


void QnDualQualityHelper::findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& catalog, DeviceFileCatalog::FindMethod findMethod, bool preciseFind)
{
    //qDebug() << "find data for time=" << QDateTime::fromMSecsSinceEpoch(time).toString(QLatin1String("hh:mm:ss.zzz"));

    for (int i = 0; i < 2; ++i) // try Normal storages first, then Backup.
    {
        bool usePreciseFind = m_alreadyOnAltChunk || preciseFind;
        m_alreadyOnAltChunk = false;

        catalog = (m_quality == MEDIA_Quality_Low ? m_catalogLow[i] : m_catalogHi[i]);
        if (catalog == 0)
            return; // no data in archive

        chunk = catalog->chunkAt(catalog->findFileIndex(time, findMethod));

        qint64 timeDistance = chunk.distanceToTime(time);

        if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && chunk.endTimeMs() <= time)
            timeDistance = INT64_MAX; // actually chunk not found
        if (timeDistance > 0)
        {
            // chunk not found. check in alternate quality
            DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_Low ? m_catalogHi[i] : m_catalogLow[i]);
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
                break;
            }
        }
        else
            return;
    }
}

void QnDualQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    if (m_quality != quality) {
        m_quality = quality;
        m_alreadyOnAltChunk = false;
    }
}
