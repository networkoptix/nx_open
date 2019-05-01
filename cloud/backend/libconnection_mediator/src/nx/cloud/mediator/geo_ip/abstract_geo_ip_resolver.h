#pragma once

#include <string>

namespace nx::hpm::geo_ip {

enum ResultCode
{
    ok,
    ioError,
    unknownError
};

enum class Continent
{
    notFound,
    africa,
    antarctica,
    asia,
    australia,
    europe,
    northAmerica,
    southAmerica
};

const char* toString(Continent continent);

//-------------------------------------------------------------------------------------------------
// AbstractGeoIpResolver

class AbstractGeoIpResolver
{
public:
    virtual ~AbstractGeoIpResolver() = default;

    /**
     * Get the continent this ip address belongs to.
     */
     virtual std::pair<ResultCode, Continent> resolve(const std::string& ipAddress) = 0;
};

} // namespace nx::hpm::geo_ip