#include "types.h"

#include <cmath>

namespace nx::geo_ip {

namespace {

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDegreesToRadians = kPi / 180;
static constexpr Kilometers kEarthDiameter = 6371 * 2;

double toRadians(double degrees)
{
    return degrees * kDegreesToRadians;
}

const char* continentString(Continent continent)
{
    return toString(continent);
}

} // namespace

bool Geopoint::isValid() const
{
    return
        latitude >= -180 && latitude <= 180
        && longitude >= -180 && longitude <= 180;
}

Kilometers Geopoint::distanceTo(const Geopoint& other) const
{
    double lat1r = toRadians(latitude);
    double lon1r = toRadians(longitude);
    double lat2r = toRadians(other.latitude);
    double lon2r = toRadians(other.longitude);
    double u = std::sin((lat2r - lat1r) / 2);
    double v = std::sin((lon2r - lon1r) / 2);
    return
        kEarthDiameter * std::asin(std::sqrt(u * u + std::cos(lat1r) * std::cos(lat2r) * v * v));
}

std::string Geopoint::toString() const
{
    return "{ lat: " + std::to_string(latitude) + ", lon: " + std::to_string(longitude) + " }";
}

const char* toString(Continent continent)
{
    switch (continent)
    {
        case Continent::africa:
            return "Africa";
        case Continent::antarctica:
            return "Antarctica";
        case Continent::asia:
            return "Asia";
        case Continent::australia:
            return "Australia";
        case Continent::europe:
            return "Europe";
        case Continent::northAmerica:
            return "North America";
        case Continent::southAmerica:
            return "South America";
        default:
            return "unknown";
    }
}

Exception::Exception(ResultCode resultCode, const std::string& message):
    base_type(message),
    m_resultCode(resultCode)
{
}

ResultCode Exception::resultCode() const
{
    return m_resultCode;
}

Location::Location(Continent continent):
    continent(continent),
    geopoint(kContinentalCenters[continent])
{
}

std::string Country::toString() const
{
    if (name.empty() && subdivision.empty())
        return {};

    std::string s;
    s.reserve(name.size() + subdivision.size() + 6);
    return s.append("{ ")
        .append(name)
        .append(!name.empty() && !subdivision.empty() ?  ", " : "")
        .append(subdivision).append(" }");
}

std::string Location::toString() const
{
    std::string geopointStr = geopoint.toString();
    std::string countryStr = country.toString();

    //22 == length of all formatting characters + size of longest continent name
    std::string s;
    s.reserve(geopointStr.size() + countryStr.size() + city.size() + 22);
    return s.append("{ ")
        .append(continentString(continent)).append(", ")
        .append(geopointStr).append((!countryStr.empty() ? ", " : ""))
        .append(countryStr).append(!city.empty() ? ", " : "")
        .append(city).append(" }");
}

} // namespace nx::geo_ip
