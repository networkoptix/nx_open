#pragma once

#include "abstract_geo_ip_resolver.h"

namespace nx::hpm::geo_ip {

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

} // namespace nx::hpm::geo_ip
