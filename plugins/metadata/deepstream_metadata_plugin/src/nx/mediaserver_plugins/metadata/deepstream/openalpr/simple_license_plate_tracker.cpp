#include "simple_license_plate_tracker.h"

#include <nx/mediaserver_plugins/metadata/deepstream/utils.h>
#include <nx/mediaserver_plugins/metadata/deepstream/deepstream_metadata_plugin_ini.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
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
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx


