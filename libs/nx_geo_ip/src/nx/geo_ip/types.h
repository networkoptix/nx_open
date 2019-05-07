#pragma once

#include <string>
#include <optional>

namespace nx::geo_ip {

enum class ResultCode
{
    ok,
    notFound,
    ioError,
    unknownError
};

enum class Continent
{
    africa,
    antarctica,
    asia,
    australia,
    europe,
    northAmerica,
    southAmerica
};

struct Geopoint
{
    double latitude = 0;
    double longitude = 0;

    std::string toString() const
    {
        return "{ lat: " + std::to_string(latitude) + ", lon: " + std::to_string(longitude) + " }";
    }
};

struct Location
{
    Continent continent;
    std::string country;
    std::optional<Geopoint> geopoint;
};

using Result = std::pair<ResultCode, Location>;

//-------------------------------------------------------------------------------------------------

NX_GEO_IP_API const char * toString(Continent continent);

} // namespace geo_ip::types