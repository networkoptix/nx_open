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
    for(QnMotionArchive* writer: m_writers.values())
        delete writer;
    m_writers.clear();
}

QnMotionArchive* QnMotionHelper::getArchive(const QnResourcePtr& res, int channel)
{
    QnMutexLocker lock( &m_mutex );
    QnNetworkResourcePtr netres = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (!netres)
        return 0;
    QnMotionArchive* writer = m_writers.value(MotionArchiveKey(netres, channel));
    if (writer == 0) {
        writer = new QnMotionArchive(m_dataDir, netres->getUniqueId(), channel);
        m_writers.insert(MotionArchiveKey(netres, channel), writer);
    }
    return writer;
}

void QnMotionHelper::saveToArchive(const QnConstMetaDataV1Ptr& data)
{
    QnMotionArchive* archive = getArchive(data->dataProvider->getResource(), data->channelNumber);
    if (archive)
        archive->saveToArchive(data);

}

QnMotionArchiveConnectionPtr QnMotionHelper::createConnection(const QnResourcePtr& res, int channel)
{
    QnMotionArchive* archive = getArchive(res, channel);
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
                QnMotionArchive* archive = getArchive(res, i);
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
