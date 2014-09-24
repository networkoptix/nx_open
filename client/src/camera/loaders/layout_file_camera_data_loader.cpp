#include "layout_file_camera_data_loader.h"

#include <api/serializer/serializer.h>

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>
#include <camera/data/bookmark_camera_data.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period_list.h>

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent)
{

}

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(const QnResourcePtr &resource, Qn::CameraDataType dataType, const QnAbstractCameraDataPtr& data, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent),
    m_data(data)
{

}


QnLayoutFileCameraDataLoader::~QnLayoutFileCameraDataLoader()
{
    //qFreeAligned(m_motionData);
}

QnLayoutFileCameraDataLoader* QnLayoutFileCameraDataLoader::newInstance(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent)
{
    QnAviResourcePtr localFile = resource.dynamicCast<QnAviResource>();
    if (!localFile)
        return NULL;
    QnLayoutFileStorageResourcePtr storage = localFile->getStorage().dynamicCast<QnLayoutFileStorageResource>();
    if (!storage)
        return NULL;

    switch (dataType) {
    case Qn::RecordedTimePeriod: 
        {
            QnTimePeriodList chunks = storage->getTimePeriods(resource);
            return new QnLayoutFileCameraDataLoader(resource, dataType, QnAbstractCameraDataPtr(new QnTimePeriodCameraData(chunks)), parent);
        }
    default:
        return new QnLayoutFileCameraDataLoader(resource, dataType, parent);
    }
   
}

int QnLayoutFileCameraDataLoader::loadChunks(const QnTimePeriod &period)
{
    Q_UNUSED(period)
    int handle = qn_fakeHandle.fetchAndAddAcquire(1);
    emit delayedReady(m_data, handle);
    return handle;
}

int QnLayoutFileCameraDataLoader::loadMotion(const QnTimePeriod &period, const QList<QRegion> &motionRegions)
{
    QVector<char*> masks;
    for (int i = 0; i < motionRegions.size(); ++i) {
        masks << (char*) qMallocAligned(MD_WIDTH * MD_HEIGHT / 8, CL_MEDIA_ALIGNMENT);
        QnMetaDataV1::createMask(motionRegions[i], masks.last());
    }

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
                if (itr->channel <= motionRegions.size() && QnMetaDataV1::matchImage((__m128i*) itr->data, (__m128i*) masks[itr->channel]))
                    periods.last() << QnTimePeriod(itr->startTimeMs, itr->durationMs);
            }
        }
    }
    QnTimePeriodList merged = QnTimePeriodList::mergeTimePeriods(periods);
    QnAbstractCameraDataPtr result(new QnTimePeriodCameraData(merged));

    for (int i = 0; i < masks.size(); ++i)
        qFreeAligned(masks[i]);


    int handle = qn_fakeHandle.fetchAndAddAcquire(1);
    emit delayedReady(result, handle);
    return handle;
}

int QnLayoutFileCameraDataLoader::load(const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs)
{
    switch (m_dataType) {
    case Qn::RecordedTimePeriod:
        return loadChunks(period);
    case Qn::MotionTimePeriod:
        {
            QList<QRegion> motionRegions;
            parseRegionList(motionRegions, filter);
            for (int i = 0; i < motionRegions.size(); ++i) 
            {
                if (!motionRegions[i].isEmpty())
                    return loadMotion(period, motionRegions);
            }
            qWarning() << "empty motion region";
        }
        //TODO: #GDM #Bookmarks intended fall-through to get assert, implement saving and loading bookmarks in layouts and files
    default:
        assert(false);
    }
    return 0; //should never get here
}
