#include "simple_license_plate_tracker.h"

#include <nx/sdk/helpers/uuid_helper.h>

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
    {
        m_licensePlates[plateNumber] =
            LicensePlateInfo(plateNumber, nx::sdk::UuidHelper::randomUuid(), currentTime);
    }

    auto& licensePlateInfo = m_licensePlates.at(plateNumber);
    if ((currentTime  - licensePlateInfo.lastAppearanceTime).count()
        >= ini().licensePlateLifetimeMs)
    {
        licensePlateInfo.uuid = nx::sdk::UuidHelper::randomUuid();
    }

    licensePlateInfo.lastAppearanceTime = currentTime;
    return licensePlateInfo;
}

void SimpleLicensePlateTracker::onNewFrame()
{
    for (auto& entry: m_licensePlates)
    {
        auto& info = entry.second;
        info.justDetected = false;
    }
}

std::vector<LicensePlateInfo> SimpleLicensePlateTracker::takeOutdatedLicensePlateInfos()
{
    auto currentTime = now();
    std::vector<LicensePlateInfo> result;
    for (auto it = m_licensePlates.begin(); it != m_licensePlates.end();)
    {
        const auto& info  = it->second;
        if ((currentTime - info.lastAppearanceTime).count() >= ini().licensePlateLifetimeMs)
        {
            result.push_back(info);
            it = m_licensePlates.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return result;
}

void SimpleLicensePlateTracker::cleanUpOutdatedLicensePlateInfos()
{
    auto currentTime = now();
    for (auto it = m_licensePlates.begin(); it != m_licensePlates.end();)
    {
        const auto& info  = it->second;
        if ((currentTime - info.lastAppearanceTime).count() >= ini().licensePlateLifetimeMs)
            it = m_licensePlates.erase(it);
        else
            ++it;
    }
}

std::vector<LicensePlateInfo> SimpleLicensePlateTracker::getJustDetectedLicensePlates()
{
    std::vector<LicensePlateInfo> result;
    for (const auto& entry: m_licensePlates)
    {
        const auto& info = entry.second;
        if (info.justDetected)
            result.push_back(info);
    }

    return result;
}

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx


