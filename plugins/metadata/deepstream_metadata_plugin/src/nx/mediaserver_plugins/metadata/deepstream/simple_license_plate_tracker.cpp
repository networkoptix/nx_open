#include "simple_license_plate_tracker.h"

#include "utils.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

LicensePlateInfo SimpleLicensePlateTracker::licensePlateInfo(const std::string &plateNumber) const
{
    auto itr = m_licensePlates.find(plateNumber);
    if (itr == m_licensePlates.cend())
        m_licensePlates[plateNumber] = LicensePlateInfo(plateNumber, makeGuid());

    return m_licensePlates.at(plateNumber);
}

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx


