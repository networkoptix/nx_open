#pragma once

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdexcept>

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
    australia, //<includes Australasia
    europe,
    northAmerica,
    southAmerica
};

using Kilometers = double;

struct NX_GEO_IP_API Geopoint
{
    /* Decimal degrees [-180, 180], negative value is Southern hemisphere. */
    double latitude = 0;
    /* Decimal degrees [-180, 180], negative value is Western hemisphere. */
    double longitude = 0;

    /* True if both latitude and longitude are in the range [-180, 180] */
    bool isValid() const;

    /**
     * Assuming Earth radius of 6371 kilometers, get the distance between this point and /a other
     * using Haversine formula.
     */
    Kilometers distanceTo(const Geopoint& other) const;

    std::string toString() const;
};

static std::map<Continent, Geopoint> kContinentalCenters = {
    {Continent::africa, {0, 26.121}},
    {Continent::antarctica, {-90, 0}},
    {Continent::asia, {43.681, 87.331}},
    {Continent::australia, {-25.61, 134.354}},
    {Continent::europe, {55.182, 28.258}},
    {Continent::northAmerica, {47.115, -101.298}},
    {Continent::southAmerica, {-20.719, -58.061}}
};

struct NX_GEO_IP_API Country
{
    /** United States, United Kingdom, etc*/
    std::string name;
    /** State in the case of U.S., "London" or "Ireland" in the case of the U.K>, etc. */
    std::string subdivision;

    std::string toString() const;
};

struct NX_GEO_IP_API Location
{
    Continent continent = Continent::africa;
    Geopoint geopoint = kContinentalCenters[continent];
    Country country;
    std::string city;

    Location() = default;
    /**
     * Constructs Location with the given continent and
     * geopoint at kContinentalCenters[continent]
     */
    Location(Continent continent);

    std::string toString() const;
};

class Exception: public std::runtime_error
{
    using base_type = std::runtime_error;
public:
    Exception(ResultCode resultCode, const std::string& mmdbError);

    ResultCode resultCode() const;

private:
    ResultCode m_resultCode;
};

//-------------------------------------------------------------------------------------------------

NX_GEO_IP_API const char* toString(Continent continent);

} // namespace geo_ip