#include "layout_file_camera_data_loader.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>
#include <camera/data/bookmark_camera_data.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period_list.h>

#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent)
{

}

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::CameraDataType dataType, const QnAbstractCameraDataPtr& data, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent),
    m_data(data)
{

}


QnLayoutFileCameraDataLoader::~QnLayoutFileCameraDataLoader()
{
    //qFreeAligned(m_motionData);
}

QnLayoutFileCameraDataLoader* QnLayoutFileCameraDataLoader::newInstance(const QnAviResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent)
{
    QnLayoutFileStorageResourcePtr storage = resource->getStorage().dynamicCast<QnLayoutFileStorageResource>();
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

int QnLayoutFileCameraDataLoader::loadChunks()
{
    int handle = qn_fakeHandle.fetchAndAddAcquire(1);
    emit delayedReady(m_data, QnTimePeriod(0, QnTimePeriod::infiniteDuration()), handle);
    return handle;
}

int QnLayoutFileCameraDataLoader::loadMotion(const QList<QRegion> &motionRegions)
{
    QVector<char*> masks;
    for (int i = 0; i < motionRegions.size(); ++i) {
        masks << (char*) qMallocAligned(MD_WIDTH * MD_HEIGHT / 8, CL_MEDIA_ALIGNMENT);
        QnMetaDataV1::createMask(motionRegions[i], masks.last());
    }

    QnAviResourcePtr aviRes = m_resource.dynamicCast<QnAviResource>();
    if (!aviRes)
        return -1;
    std::vector<QnTimePeriodList> periods;
    for (int channel = 0; channel < motionRegions.size(); ++channel)
    {
        const QnMetaDataLightVector& m_motionData = aviRes->getMotionBuffer(channel);
        if (!m_motionData.empty())
        {
            periods.push_back(QnTimePeriodList());

            for (auto itr = m_motionData.begin(); itr != m_motionData.end(); ++itr)
            {
                if (itr->channel <= motionRegions.size() && QnMetaDataV1::matchImage((__m128i*) itr->data, (__m128i*) masks[itr->channel]))
                    periods.rbegin()->push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));
            }
        }
    }
    QnTimePeriodList merged = QnTimePeriodList::mergeTimePeriods(periods);
    QnAbstractCameraDataPtr result(new QnTimePeriodCameraData(merged));

    for (int i = 0; i < masks.size(); ++i)
        qFreeAligned(masks[i]);


    int handle = qn_fakeHandle.fetchAndAddAcquire(1);
    emit delayedReady(result, QnTimePeriod(0, QnTimePeriod::infiniteDuration()), handle);
    return handle;
}

int QnLayoutFileCameraDataLoader::load(const QString &filter, const qint64 resolutionMs) {
    Q_UNUSED(resolutionMs)

    switch (m_dataType) {
    case Qn::RecordedTimePeriod:
        return loadChunks();
    case Qn::MotionTimePeriod:
        {
            QList<QRegion> motionRegions = QJson::deserialized<QList<QRegion>>(filter.toUtf8());
            for (const QRegion &region: motionRegions) 
            {
                if (!region.isEmpty())
                    return loadMotion(motionRegions);
            }
            qWarning() << "empty motion region";
        }
        //TODO: #GDM #Bookmarks intended fall-through to get assert, implement saving and loading bookmarks in layouts and files
    default:
      ///  Q_ASSERT(false);
        return 0;
    }
    return -1; //should never get here
}
