#pragma once

#include <map>
#include <string>
#include <chrono>

#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

struct LicensePlateInfo
{
    LicensePlateInfo() = default;
    LicensePlateInfo(
        const std::string& plateNumber,
        const nxpl::NX_GUID& guid,
        const std::chrono::milliseconds& detectionTime)
        :
        plateNumber(plateNumber),
        guid(guid),
        lastAppearanceTime(detectionTime)
    {
    }

    std::string plateNumber;
    nxpl::NX_GUID guid;
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
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
