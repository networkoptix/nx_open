#pragma once

#include <map>
#include <string>
#include <chrono>

#include <nx/sdk/uuid.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

struct LicensePlateInfo
{
    LicensePlateInfo() = default;
    LicensePlateInfo(
        const std::string& plateNumber,
        const nx::sdk::Uuid& uuid,
        const std::chrono::milliseconds& detectionTime)
        :
        plateNumber(plateNumber),
        uuid(uuid),
        lastAppearanceTime(detectionTime)
    {
    }

    std::string plateNumber;
    nx::sdk::Uuid uuid;
    std::chrono::milliseconds lastAppearanceTime;
};

class SimpleLicensePlateTracker
{
public:
    LicensePlateInfo licensePlateInfo(const std::string& plateNumber) const;

private:
    mutable std::map<std::string, LicensePlateInfo> m_licensePlates;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
