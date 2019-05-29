#include "analytics_helper.h"

#include <motion/motion_detection.h>
#include "analytics_archive.h"
#include <core/resource/camera_resource.h>

namespace nx::vms::server::metadata {

QnTimePeriodList AnalyticsHelper::matchImage(
    const QnChunksRequestData& request)
{
    std::vector<QnTimePeriodList> timePeriods;
    for (const auto& res: request.resList)
    {
        const auto motionRegions = regionsFromFilter(
            request.filter, res->getVideoLayout()->channelCount());

        if (res->isDtsBased())
        {
            NX_ASSERT(0, "Not implemented");
            return QnTimePeriodList();
        }

        AnalyticsArchive archive(m_dataDir, res->getPhysicalId());

        // TODO: #akolesnikov fill it here from filter.
        std::vector<int64_t> objectTypes;
        std::vector<int64_t> allAttributesHash;

        // Analytics not use separate files for different video channels.
        // Use motionRegions[0] only because of that.
        timePeriods.push_back(archive.matchPeriod(
            {
                motionRegions[0],
                std::chrono::milliseconds(request.startTimeMs),
                std::chrono::milliseconds(request.endTimeMs),
                request.detailLevel,
                request.limit,
                request.sortOrder,
                objectTypes,
                allAttributesHash
            }));
    }
    return QnTimePeriodList::mergeTimePeriods(timePeriods, request.limit, request.sortOrder);
}

} // namespace nx::vms::server::metadata
