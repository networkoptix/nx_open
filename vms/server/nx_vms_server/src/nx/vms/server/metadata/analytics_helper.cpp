#include "analytics_helper.h"

#include <motion/motion_detection.h>
#include "analytics_archive.h"
#include <core/resource/camera_resource.h>

namespace nx::vms::server::metadata {

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

} // namespace nx::vms::server::metadata
