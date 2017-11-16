#include "net_dimensions.h"

#include <cstring>

#include <nx/kit/debug.h>

/**
 * Parse a natural number from string like "...dim: 1024".
 * @return -1 on error.
 */
static int extractNetDimension(const std::string& s)
{
    static const char* const kPrefix = "dim:";

    const auto pos = s.find(kPrefix);
    if (pos == std::string::npos)
        return -1;

    errno = 0; //< Required before strtol().

    const int value = (int) std::strtol(
        s.c_str() + pos + strlen(kPrefix), /*endptr*/ nullptr, /*base*/ 10);

    if (errno != 0)
        return -1;

    return value;
}

NetDimensions getNetDimensions(
    std::istream& input, const std::string& filename, NetDimensions specifiedDimensions)
{
    if (specifiedDimensions.width > 0 && specifiedDimensions.height > 0)
        return specifiedDimensions;

    static const NetDimensions error{0, 0};
    NetDimensions result{0, 0};

    if (input.fail())
    {
        NX_PRINT << "ERROR: Unable to open \"" << filename << "\" to parse net dimensions.";
        return error;
    }

    int valueIndex = 0;
    while (!input.eof())
    {
        std::string s;
        std::getline(input, s);
        if (input.fail())
        {
            NX_PRINT << "ERROR: Unable to read from \"" << filename
                     << "\" to parse net dimensions.";
            return error;
        }

        const int value = extractNetDimension(s);
        if (value <= 0)
            continue;

        if (valueIndex == 2) //< height
        {
            result.height = value;
        }
        else if (valueIndex == 3) //< width
        {
            result.width = value;
            break; //< Width is the last needed value - no more parsing is required.
        }

        ++valueIndex;
    }

    if (result.width <= 0 || result.height <= 0)
    {
        NX_PRINT << "ERROR: Unable to parse net width and/or height from \"" << filename << "\".";
        return error;
    }
    return result;
}
