#pragma once

#include "types.h"

namespace nx::geo_ip {

const char * toString(Continent continent)
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
            return "unknown"; //< Should never get here, Continent is an enum class.
    }
}

} // namespace nx::geo_ip
