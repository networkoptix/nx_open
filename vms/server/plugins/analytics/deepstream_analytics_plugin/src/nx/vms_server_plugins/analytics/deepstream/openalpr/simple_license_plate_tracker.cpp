#include "simple_license_plate_tracker.h"

#include <nx/vms_server_plugins/analytics/deepstream/utils.h>
#include <nx/vms_server_plugins/analytics/deepstream/deepstream_analytics_plugin_ini.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

LicensePlateInfo SimpleLicensePlateTracker::licensePlateInfo(const std::string &plateNumber) const
{
    auto itr = m_licensePlates.find(plateNumber);
    auto currentTime = now();
    if (itr == m_licensePlates.cend())
        m_licensePlates[plateNumber] = LicensePlateInfo(plateNumber, makeGuid(), currentTime);

    auto& licensePlateInfo = m_licensePlates.at(plateNumber);
    if ((currentTime  - licensePlateInfo.lastAppearanceTime).count() > ini().licensePlateLifetimeMs)
        licensePlateInfo.guid = makeGuid();

    licensePlateInfo.lastAppearanceTime = currentTime;
    return licensePlateInfo;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx


