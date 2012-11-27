#include "dualquality_helper.h"
#include "recorder/storage_manager.h"

static const int SECOND_STREAM_FIND_EPS = 1000 * 5;
static const int FIRST_STREAM_FIND_EPS = 1000 * 15;

QnDialQualityHelper::QnDialQualityHelper()
{
    m_quality = MEDIA_Quality_High;
    m_alreadyOnAltChunk = false;
}

void QnDialQualityHelper::setResource(QnNetworkResourcePtr netResource)
{
    m_catalogHi = qnStorageMan->getFileCatalog(netResource->getPhysicalId(), QnResource::Role_LiveVideo);
    m_catalogLow = qnStorageMan->getFileCatalog(netResource->getPhysicalId(), QnResource::Role_SecondaryLiveVideo);
}

void QnDialQualityHelper::findDataForTime(const qint64 time, DeviceFileCatalog::Chunk& chunk, DeviceFileCatalogPtr& catalog, DeviceFileCatalog::FindMethod findMethod)
{
    //qDebug() << "find data for time=" << QDateTime::fromMSecsSinceEpoch(time).toString(QLatin1String("hh:mm:ss.zzz"));
    bool altChunkWasSelected = m_alreadyOnAltChunk;
    m_alreadyOnAltChunk = false;

    catalog = (m_quality == MEDIA_Quality_Low ? m_catalogLow : m_catalogHi);
    if (catalog == 0)
        return; // no data in archive

    chunk = catalog->chunkAt(catalog->findFileIndex(time, findMethod));

    qint64 timeDistance = chunk.distanceToTime(time);

    if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && chunk.endTimeMs() <= time)
        timeDistance = INT_MAX; // actually chunk not found
    if (timeDistance > 0)
    {
        // chunk not found. check in alternate quality
        DeviceFileCatalogPtr catalogAlt = (m_quality == MEDIA_Quality_Low ? m_catalogHi : m_catalogLow);
        DeviceFileCatalog::Chunk altChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(time, findMethod));
        qint64 timeDistanceAlt = altChunk.distanceToTime(time);
        if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs() <= time)
            timeDistanceAlt = INT_MAX; // actually chunk not found

        int findEps = m_quality == MEDIA_Quality_High ? SECOND_STREAM_FIND_EPS : FIRST_STREAM_FIND_EPS;
        if (timeDistance - timeDistanceAlt > findEps || (timeDistance > 0 && altChunkWasSelected))
        {
            if (timeDistanceAlt == 0)
            {
                // if recording hole, alt quality chunk may be slighthy longer then primary. So, distance to such chunk still 0
                // prevent change quality, if such chunk rest very low
                if (findMethod == DeviceFileCatalog::OnRecordHole_NextChunk && altChunk.endTimeMs()-time < findEps)
                {
                    DeviceFileCatalog::Chunk nextAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.endTimeMs(), findMethod));
                    //if (nextAltChunk.startTimeMs != altChunk.endTimeMs())
                    if (nextAltChunk.startTimeMs > chunk.startTimeMs - findEps)
                        return;
                    if (nextAltChunk.startTimeMs == altChunk.startTimeMs)
                        return; // EOF reached
                }
                else if (findMethod == DeviceFileCatalog::OnRecordHole_PrevChunk && time - altChunk.startTimeMs < findEps)
                {
                    DeviceFileCatalog::Chunk prevAltChunk = catalogAlt->chunkAt(catalogAlt->findFileIndex(altChunk.startTimeMs-1, findMethod));
                    //if (prevAltChunk.endTimeMs() != altChunk.startTimeMs)
                    if (prevAltChunk.endTimeMs() < chunk.endTimeMs() + findEps)
                        return;
                }
            }

            // alternate quality matched better
            if (timeDistance != INT_MAX && altChunk.containsTime(chunk.startTimeMs))
                altChunk.truncate(chunk.startTimeMs); // truncate to start of the next chunk of the required quality (if next chunk of requested quality is exists)

            catalog = catalogAlt;
            chunk = altChunk;
            m_alreadyOnAltChunk = true;
        }
    }
}

void QnDialQualityHelper::setPrefferedQuality(MediaQuality quality)
{
    if (m_quality != quality) {
        m_quality = quality;
        m_alreadyOnAltChunk = false;
    }
}
