#include "dualquality_helper.h"
#include "recorder/storage_manager.h"

static const int SECOND_STREAM_FIND_EPS = 1000 * 5;

QnDialQualityHelper::QnDialQualityHelper()
{
    m_quality = MEDIA_Quality_High;
}

void QnDialQualityHelper::setResource(QnNetworkResourcePtr netResource)
{
    m_catalogHi = qnStorageMan->getFileCatalog(netResource->getPhysicalId(), QnResource::Role_LiveVideo);
    m_catalogLow = qnStorageMan->getFileCatalog(netResource->getPhysicalId(), QnResource::Role_SecondaryLiveVideo);
}

void QnDialQualityHelper::findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& catalog, DeviceFileCatalog::FindMethod findMethod) const
{
    catalog = (m_quality == MEDIA_Quality_High ? m_catalogHi : m_catalogLow);
    if (catalog == 0)
        return; // no data in archive

    chunk = catalog->chunkAt(catalog->findFileIndex(time, findMethod));

    qint64 timeDistance = chunk.distanceToTime(time);
    if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && chunk.endTimeMs() <= time)
        timeDistance = INT_MAX; // actually chunk not found
    if (timeDistance > 0)
    {
        // chunk not found. check in alternate quality
        DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_High ? m_catalogLow : m_catalogHi);
        DeviceFileCatalog::Chunk altChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(time, findMethod));
        qint64 timeDistanceAlt = altChunk.distanceToTime(time);
        if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs() <= time)
            timeDistanceAlt = INT_MAX; // actually chunk not found

        if (timeDistance - timeDistanceAlt > SECOND_STREAM_FIND_EPS) 
        {
            if (timeDistanceAlt == 0 && altChunk.endTimeMs()-time < SECOND_STREAM_FIND_EPS)
            {
                // if recording hole, alt quality chunk may be slighthy longer then primary. So, distance to such chunk still 0
                // prevent change quality, if such chunk rest very low
                DeviceFileCatalog::Chunk nextAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.endTimeMs(), DeviceFileCatalog::OnRecordHole_NextChunk));
                if (nextAltChunk.startTimeMs != altChunk.endTimeMs())
                    return;
            }

            // alternate quality matched better
            if (timeDistance != INT_MAX && altChunk.containsTime(chunk.startTimeMs))
                altChunk.truncate(chunk.startTimeMs); // truncate to start of the next chunk of the required quality (if next chunk of requested quality is exists)

            catalog = catalogAlt;
            chunk = altChunk;
        }
    }
}

void QnDialQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    m_quality = quality;
}
