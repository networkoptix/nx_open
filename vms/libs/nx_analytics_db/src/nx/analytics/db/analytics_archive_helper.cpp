#include "analytics_archive_helper.h"

#include <core/resource/camera_resource.h>
#include <motion/motion_detection.h>

#include "analytics_archive.h"

namespace nx::analytics::db {

QnTimePeriodList AnalyticsHelper::matchImage(
    const QnSecurityCamResourceList& resourceList,
    const AnalyticsArchive::AnalyticsFilter& filter)
{
    std::vector<QnTimePeriodList> timePeriods;
    for (const auto& res: resourceList)
    {
        AnalyticsArchive archive(m_dataDir, res->getPhysicalId());
        timePeriods.push_back(archive.matchPeriod(filter));
    }
    return QnTimePeriodList::mergeTimePeriods(timePeriods, filter.limit, filter.sortOrder);
}

} // namespace nx::analytics::db
