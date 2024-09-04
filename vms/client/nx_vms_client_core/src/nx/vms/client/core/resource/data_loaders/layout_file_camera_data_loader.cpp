// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_file_camera_data_loader.h"

#include <core/resource/avi/avi_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/media/config.h>
#include <nx/vms/client/core/motion/motion_grid.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::core {

QnLayoutFileCameraDataLoader::QnLayoutFileCameraDataLoader(
    const QnAviResourcePtr& resource,
    Qn::TimePeriodContent dataType,
    QObject* parent)
    :
    AbstractCameraDataLoader(resource, dataType, parent),
    m_aviResource(resource)
{
    NX_ASSERT(m_aviResource, "Resource must exist");

    // Preload recording data.
    if (dataType == Qn::RecordingContent)
    {
        auto storage = resource->getStorage().dynamicCast<QnLayoutFileStorageResource>();
        if (!storage)
            return;

        m_loadedData = storage->getTimePeriods(resource);
    }
}

QnLayoutFileCameraDataLoader::~QnLayoutFileCameraDataLoader()
{
}

void QnLayoutFileCameraDataLoader::sendDataDelayed()
{
    executeDelayedParented([this]() { emit ready(0); }, this);
}

QnTimePeriodList QnLayoutFileCameraDataLoader::loadMotion(
    const MotionSelection& motionRegions) const
{
    if (!NX_ASSERT(m_aviResource))
        return QnTimePeriodList();

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

    return QnTimePeriodList::mergeTimePeriods(periods).simplified();
}

void QnLayoutFileCameraDataLoader::load(const QString& filter, const qint64 /*resolutionMs*/)
{
    if (m_dataType == Qn::MotionContent)
    {
        // Empty motion filter is treated as a full area selection.
        if (filter.isEmpty())
        {
            m_loadedData = loadMotion({{QRect(0, 0, MotionGrid::kWidth, MotionGrid::kHeight)}});
        }
        else
        {
            const auto motionRegions = QJson::deserialized<MotionSelection>(filter.toUtf8());
            m_loadedData = loadMotion(motionRegions);
        }
    }
    sendDataDelayed();
}

} // namespace nx::vms::client::core
