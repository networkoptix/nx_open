#include "dualquality_helper.h"
#include "recorder/storage_manager.h"

static const int SECOND_STREAM_FIND_EPS = 1000 * 5;

QnDialQualityHelper::QnDialQualityHelper()
{
    m_quality = MEDIA_Quality_High;
    m_findMethod = DeviceFileCatalog::OnRecordHole_NextChunk;
}

void QnDialQualityHelper::setResource(QnNetworkResourcePtr netResource)
{
    m_catalogHi = qnStorageMan->getFileCatalog(netResource->getMAC().toString(), QnResource::Role_LiveVideo);
    m_catalogLow = qnStorageMan->getFileCatalog(netResource->getMAC().toString(), QnResource::Role_SecondaryLiveVideo);
}

void QnDialQualityHelper::findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& catalog)
{
    catalog = (m_quality == MEDIA_Quality_High ? m_catalogHi : m_catalogLow);
    chunk = catalog->chunkAt(catalog->findFileIndex(time, m_findMethod));

    qint64 timeDistance = chunk.distanceToTime(time);
    if (timeDistance > 0)
    {
        // chunk not found. check in alternate quality
        DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_High ? m_catalogLow : m_catalogHi);
        DeviceFileCatalog::Chunk altChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(time, m_findMethod));
        qint64 timeDistanceAlt = altChunk.distanceToTime(time);
        if (timeDistance - timeDistanceAlt > SECOND_STREAM_FIND_EPS) 
        {
            // alternate quality matched better
            catalog = catalogAlt;
            chunk = altChunk;
        }
    }
}

void QnDialQualityHelper::setFindMethod(DeviceFileCatalog::FindMethod findMethod)
{
    m_findMethod = findMethod;
}

void QnDialQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    m_quality = quality;
}
