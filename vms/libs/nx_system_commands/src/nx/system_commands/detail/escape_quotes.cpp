#include "escape_quotes.h"

#include <sstream>

namespace nx::system_commands {

std::string escapeSingleQuotes(const std::string& arg)
{
    std::stringstream ss;
    for (const char c: arg)
    {
        if (c == '\'')
            ss << "'\\''";
        else
            ss << c;
    }

    return ss.str();
}

} // namespace nx::system_commands
