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
            // alternate quality matched better
            if (altChunk.containsTime(chunk.startTimeMs))
                altChunk.truncate(chunk.startTimeMs);

            catalog = catalogAlt;
            chunk = altChunk;
        }
    }
}

void QnDialQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    m_quality = quality;
}
