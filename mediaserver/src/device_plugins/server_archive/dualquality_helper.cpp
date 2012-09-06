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
    catalog = (m_quality == MEDIA_Quality_Low ? m_catalogLow : m_catalogHi);
    if (catalog == 0)
        return; // no data in archive

    chunk = catalog->chunkAt(catalog->findFileIndex(time, findMethod));

    qint64 timeDistance = chunk.distanceToTime(time);
    if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && chunk.endTimeMs() <= time)
        timeDistance = INT_MAX; // actually chunk not found
    if (timeDistance > 0 && m_quality != MEDIA_Quality_AlwaysHigh)
    {
        // chunk not found. check in alternate quality
        DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_Low ? m_catalogHi : m_catalogLow);
        DeviceFileCatalog::Chunk altChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(time, findMethod));
        qint64 timeDistanceAlt = altChunk.distanceToTime(time);
        if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs() <= time)
            timeDistanceAlt = INT_MAX; // actually chunk not found

        if (timeDistance - timeDistanceAlt > SECOND_STREAM_FIND_EPS) 
        {
            if (timeDistanceAlt == 0)
            {
                // if recording hole, alt quality chunk may be slighthy longer then primary. So, distance to such chunk still 0
                // prevent change quality, if such chunk rest very low
                if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs()-time < SECOND_STREAM_FIND_EPS)
                {
                    DeviceFileCatalog::Chunk nextAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.endTimeMs(), findMethod));
                    if (nextAltChunk.startTimeMs != altChunk.endTimeMs())
                        return;
                }
                else if (findMethod == DeviceFileCatalog::OnRecordHole_PrevChunk && time - altChunk.startTimeMs < SECOND_STREAM_FIND_EPS)
                {
                    DeviceFileCatalog::Chunk prevAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.startTimeMs-1, findMethod));
                    if (prevAltChunk.endTimeMs() != altChunk.startTimeMs)
                        return;
                }
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
