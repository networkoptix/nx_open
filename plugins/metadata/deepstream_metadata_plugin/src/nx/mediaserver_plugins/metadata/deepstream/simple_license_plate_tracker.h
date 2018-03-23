#pragma once

#include <map>
#include <string>

#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

struct LicensePlateInfo
{
    LicensePlateInfo() = default;
    LicensePlateInfo(const std::string& plateNumber, const nxpl::NX_GUID& guid):
        plateNumber(plateNumber),
        guid(guid)
    {
    }

    std::string plateNumber;
    nxpl::NX_GUID guid;
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
