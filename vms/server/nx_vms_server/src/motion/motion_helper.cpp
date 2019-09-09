#include "motion_helper.h"
#include "motion_archive.h"
#include <nx/streaming/abstract_stream_data_provider.h>
#include "utils/common/util.h"
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include "recorder/file_deletor.h"

#include <core/resource/camera_resource.h>
#include <recording/time_period_list.h>

// ------------------------ QnMotionHelper ------------------------

QnMotionHelper::QnMotionHelper(const QString& dataDir, QObject* parent):
    MetadataHelper(dataDir, parent)
{
}

QnMotionHelper::~QnMotionHelper()
{
    QnMutexLocker lock( &m_mutex );
    m_writers.clear();
}

void QnMotionHelper::remove(const QnResourcePtr& res)
{
    const auto networkResource = res.dynamicCast<QnNetworkResource>();
    if (!networkResource)
        return;

    QnMutexLocker lock(&m_mutex);
    const auto left = m_writers.lower_bound(MotionArchiveKey(networkResource, 0));
    const auto right = m_writers.upper_bound(MotionArchiveKey(networkResource, CL_MAX_CHANNELS));
    m_writers.erase(left, right);
}

QnMotionArchivePtr QnMotionHelper::getArchive(const QnResourcePtr& res, int channel)
{
    QnMutexLocker lock( &m_mutex );
    QnNetworkResourcePtr netres = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (!netres)
        return 0;
    auto itr = m_writers.find(MotionArchiveKey(netres, channel));
    if (itr != m_writers.end())
        return itr->second;

    auto writer = std::make_shared<QnMotionArchive>(m_dataDir, netres->getUniqueId(), channel);
    m_writers.emplace(MotionArchiveKey(netres, channel), writer);
    return writer;
}

void QnMotionHelper::saveToArchive(const QnConstMetaDataV1Ptr& data)
{
    auto archive = getArchive(data->dataProvider->getResource(), data->channelNumber);
    if (archive)
        archive->saveToArchive(data);

}

QnMotionArchiveConnectionPtr QnMotionHelper::createConnection(const QnResourcePtr& res, int channel)
{
    auto archive = getArchive(res, channel);
    if (archive)
        return archive->createConnection();
    else
        return QnMotionArchiveConnectionPtr();
}

QnTimePeriodList QnMotionHelper::matchImage(const QnChunksRequestData& request)
{
    std::vector<QnTimePeriodList> timePeriods;
    for (const auto& res: request.resList)
    {
        const auto motionRegions = regionsFromFilter(
            request.filter, res->getVideoLayout()->channelCount());

        if (res->isDtsBased())
        {
            timePeriods.push_back(res->getDtsTimePeriodsByMotionRegion(
                motionRegions,
                request.startTimeMs,
                request.endTimeMs,
                request.detailLevel.count(),
                request.keepSmallChunks,
                request.limit,
                request.sortOrder));
        }
        else
        {
            for (int i = 0; i < motionRegions.size(); ++i)
            {
                auto archive = getArchive(res, i);
                if (archive)
                {
                    timePeriods.push_back(archive->matchPeriod(
                        {
                            motionRegions[i],
                            std::chrono::milliseconds(request.startTimeMs),
                            std::chrono::milliseconds(request.endTimeMs),
                            request.detailLevel,
                            request.limit,
                            request.sortOrder
                        }));
                }
            }
        }
    }
    return QnTimePeriodList::mergeTimePeriods(timePeriods, request.limit, request.sortOrder);
}
