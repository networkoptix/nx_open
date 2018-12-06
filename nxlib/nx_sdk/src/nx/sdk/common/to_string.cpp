#include "to_string.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace common {

static const int kLevelIndent = 4;

std::string indent(int overallIndent, int level)
{
    return std::string(overallIndent, ' ') + std::string(kLevelIndent * level, ' ');
}

std::string toString(const Settings* settings, int overallIndent)
{
    if (!settings)
    {
        NX_KIT_ASSERT(settings);
        return "[]\n";
    }

    std::string result = indent(overallIndent, 0) + "[\n";

    const int count = settings->count();
    for (int i = 0; i < count; ++i)
    {
        result += indent(overallIndent, 1) + "{ "
            + "\"name\": " + nx::kit::debug::toString(settings->key(i)) + ", "
            + "\"value\": " + nx::kit::debug::toString(settings->value(i))
            + " }" + ((i < count - 1) ? "," : "") + "\n";
    }

    result += indent(overallIndent, 0) + "]\n";
    return result;
}

} // namespace common
} // namespace sdk
} // namespace nx
