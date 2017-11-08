#include "layout_file_camera_data_loader.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <core/resource/avi/avi_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <motion/motion_detection.h>

#include <recording/time_period_list.h>

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>

#include <nx/streaming/config.h>

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::TimePeriodContent dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent),
    m_aviResource(resource)
{
    NX_ASSERT(m_aviResource, Q_FUNC_INFO, "Resource must exist");

    /* Preload recording data. */
    if (dataType != Qn::RecordingContent)
        return;

    QnLayoutFileStorageResourcePtr storage = resource->getStorage().dynamicCast<QnLayoutFileStorageResource>();
    if (!storage)
        return;

    QnTimePeriodList chunks = storage->getTimePeriods(resource);
    m_data = QnAbstractCameraDataPtr(new QnTimePeriodCameraData(chunks));
}

QnLayoutFileCameraDataLoader::~QnLayoutFileCameraDataLoader()
{}


int QnLayoutFileCameraDataLoader::sendDataDelayed(const QnAbstractCameraDataPtr& data) {
    int handle = qn_fakeHandle.fetchAndAddAcquire(1);
    emit delayedReady(data, QnTimePeriod(0, QnTimePeriod::infiniteDuration()), handle);
    return handle;
}

int QnLayoutFileCameraDataLoader::loadMotion(const QList<QRegion> &motionRegions)
{
    if (!m_aviResource)
        return -1;

    QVector<char*> masks;
    for (int i = 0; i < motionRegions.size(); ++i) {
        masks << (char*) qMallocAligned(Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8, CL_MEDIA_ALIGNMENT);
        QnMetaDataV1::createMask(motionRegions[i], masks.last());
    }

    std::vector<QnTimePeriodList> periods;
    for (int channel = 0; channel < motionRegions.size(); ++channel)
    {
        const QnMetaDataLightVector& m_motionData = m_aviResource->getMotionBuffer(channel);
        if (!m_motionData.empty())
        {
            periods.push_back(QnTimePeriodList());

            for (auto itr = m_motionData.begin(); itr != m_motionData.end(); ++itr)
            {
                if (itr->channel <= motionRegions.size() && QnMetaDataV1::matchImage((simd128i*) itr->data, (simd128i*) masks[itr->channel]))
                    periods.rbegin()->push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));
            }
        }
    }
    QnTimePeriodList merged = QnTimePeriodList::mergeTimePeriods(periods);
    QnAbstractCameraDataPtr result(new QnTimePeriodCameraData(merged));

    for (int i = 0; i < masks.size(); ++i)
        qFreeAligned(masks[i]);

    return sendDataDelayed(result);
}

int QnLayoutFileCameraDataLoader::load(const QString &filter, const qint64 resolutionMs) {
    Q_UNUSED(resolutionMs)

    switch (m_dataType) {
    case Qn::RecordingContent:
        return sendDataDelayed(m_data);
    case Qn::MotionContent:
        {
            QList<QRegion> motionRegions = QJson::deserialized<QList<QRegion>>(filter.toUtf8());
            for (const QRegion &region: motionRegions)
            {
                if (!region.isEmpty())
                    return loadMotion(motionRegions);
            }
            qWarning() << "empty motion region";
        }
    default:
        NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
        return 0;
    }
}
