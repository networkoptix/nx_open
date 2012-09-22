#include "layout_file_time_period_loader.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"

QnLayoutFileTimePeriodLoader::QnLayoutFileTimePeriodLoader(QnResourcePtr resource, QObject *parent, const QnTimePeriodList& chunks):
    QnAbstractTimePeriodLoader(resource, parent),
    m_chunks(chunks),
    m_handle(0)
    //m_motionData(motionData), 
    //m_motionDataSize(motionDataSize)
{
}

QnLayoutFileTimePeriodLoader::~QnLayoutFileTimePeriodLoader()
{
    //qFreeAligned(m_motionData);
}

QnLayoutFileTimePeriodLoader* QnLayoutFileTimePeriodLoader::newInstance(QnResourcePtr resource, QObject *parent)
{
    QnAviResourcePtr localFile = resource.dynamicCast<QnAviResource>();
    if (!localFile)
        return 0;
    QnLayoutFileStorageResourcePtr storage = localFile->getStorage().dynamicCast<QnLayoutFileStorageResource>();
    if (!storage)
        return 0;
    QFileInfo fi(resource->getName());
    QIODevice* chunkData = storage->open(QString(QLatin1String("chunk_%1.bin")).arg(fi.baseName()), QIODevice::ReadOnly);
    if (!chunkData)
        return 0;
    QnTimePeriodList chunks;
    QByteArray chunkDataArray(chunkData->readAll());
    chunks.decode(chunkDataArray);
    delete chunkData;

    return new QnLayoutFileTimePeriodLoader(resource, parent, chunks);
}

int QnLayoutFileTimePeriodLoader::loadChunks(const QnTimePeriod &period)
{
    ++m_handle;
    emit delayedReady(m_chunks, m_handle);
    return m_handle;
}

int QnLayoutFileTimePeriodLoader::loadMotion(const QnTimePeriod &period, const QList<QRegion> &motionRegions)
{
    QVector<char*> masks;
    for (int i = 0; i < motionRegions.size(); ++i) {
        masks << (char*) qMallocAligned(MD_WIDTH * MD_HEIGHT / 8, CL_MEDIA_ALIGNMENT);
        QnMetaDataV1::createMask(motionRegions[i], masks.last());
    }

    QnMetaDataV1Light startTime;
    QnMetaDataV1Light endTime;

    QnAviResourcePtr aviRes = m_resource.dynamicCast<QnAviResource>();
    if (!aviRes)
        return -1;
    QVector<QnTimePeriodList> periods;
    for (int channel = 0; channel < motionRegions.size(); ++channel)
    {
        const QnMetaDataLightVector& m_motionData = aviRes->getMotionBuffer(channel);
        if (!m_motionData.empty())
        {
            periods << QnTimePeriodList();
            QnMetaDataLightVector::const_iterator itrStart = qUpperBound(m_motionData.begin(), m_motionData.end(), period.startTimeMs);
            QnMetaDataLightVector::const_iterator itrEnd = qUpperBound(itrStart, m_motionData.end(), period.endTimeMs());

            if (itrStart > m_motionData.begin() && itrStart[-1].endTimeMs() > period.startTimeMs)
                --itrStart;

            for (QnMetaDataLightVector::const_iterator itr = itrStart; itr != itrEnd; ++itr)
            {
                if (itr->channel <= motionRegions.size() && QnMetaDataV1::mathImage((__m128i*) itr->data, (__m128i*) masks[itr->channel]))
                    periods.last() << QnTimePeriod(itr->startTimeMs, itr->durationMs);
            }
        }
    }
    QnTimePeriodList result = QnTimePeriod::mergeTimePeriods(periods);
    for (int i = 0; i < masks.size(); ++i)
        qFreeAligned(masks[i]);


    ++m_handle;
    emit delayedReady(result, m_handle);
    return m_handle;
}

int QnLayoutFileTimePeriodLoader::load(const QnTimePeriod &period, const QList<QRegion> &motionRegions)
{
    for (int i = 0; i < motionRegions.size(); ++i) 
    {
        if (!motionRegions[i].isEmpty())
            return loadMotion(period, motionRegions);
    }
    return loadChunks(period);
}
