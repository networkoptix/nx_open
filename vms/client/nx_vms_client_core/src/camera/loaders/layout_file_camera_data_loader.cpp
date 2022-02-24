// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_file_camera_data_loader.h"

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <core/resource/avi/avi_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period_list.h>

#include <nx/vms/client/core/motion/motion_grid.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/streaming/config.h>

using namespace nx::vms::client::core;

namespace {
    QAtomicInt qn_fakeHandle(INT_MAX / 2);
}

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::TimePeriodContent dataType, QObject *parent):
    QnAbstractCameraDataLoader(resource, dataType, parent),
    m_aviResource(resource)
{
    NX_ASSERT(m_aviResource, "Resource must exist");

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
    emit delayedReady(data, QnTimePeriod::anytime(), handle);
    return handle;
}

int QnLayoutFileCameraDataLoader::loadMotion(const MotionSelection& motionRegions)
{
    if (!m_aviResource)
        return -1;

    QVector<MotionGridBitMask> masks;
    masks.resize(motionRegions.size());
    for (int i = 0; i < motionRegions.size(); ++i)
        QnMetaDataV1::createMask(motionRegions[i], reinterpret_cast<char*>(&masks[i]));

    std::vector<QnTimePeriodList> periods;
    for (int channel = 0; channel < motionRegions.size(); ++channel)
    {
        const QnMetaDataLightVector& m_motionData = m_aviResource->getMotionBuffer(channel);
        if (!m_motionData.empty())
        {
            periods.push_back(QnTimePeriodList());

            for (auto itr = m_motionData.begin(); itr != m_motionData.end(); ++itr)
            {
                if (itr->channel <= motionRegions.size()
                    && QnMetaDataV1::matchImage(
                        (quint64*) itr->data,
                        reinterpret_cast<simd128i*>(&masks[itr->channel])))
                {
                    periods.rbegin()->push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));
                }
            }
        }
    }
    QnTimePeriodList merged = QnTimePeriodList::mergeTimePeriods(periods);
    QnAbstractCameraDataPtr result(new QnTimePeriodCameraData(merged));

    return sendDataDelayed(result);
}

int QnLayoutFileCameraDataLoader::load(const QString& filter, const qint64 /*resolutionMs*/)
{
    using namespace nx::vms::client::core;

    switch (m_dataType)
    {
        case Qn::RecordingContent:
        {
            return sendDataDelayed(m_data);
        }
        case Qn::MotionContent:
        {
            // Empty motion filter is treated as a full area selection.
            if (filter.isEmpty())
                return loadMotion({{QRect(0, 0, MotionGrid::kWidth, MotionGrid::kHeight)}});

            const auto motionRegions = QJson::deserialized<MotionSelection>(filter.toUtf8());
            for (const auto& region: motionRegions)
            {
                if (!region.isEmpty())
                    return loadMotion(motionRegions);
            }
            NX_ASSERT(false, "Empty motion region in exported video.");
            return 0;
        }
        case Qn::AnalyticsContent:
        {
            // Analytics export is not supported yet.
            return 0;
        }
        default:
            NX_ASSERT(false, "Should never get here");
            return 0;
    }
}
