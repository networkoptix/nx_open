#include "exceptions.h"

namespace nx::utils {

std::string unwrapNestedErrors(const std::exception& e, std::string whats)
{
    if (whats.size() > 0)
        whats += ": ";
    whats += e.what();

    try
    {
        std::rethrow_if_nested(e);
    }
    catch (const std::exception& e)
    {
       return unwrapNestedErrors(e, std::move(whats));
    }
    return whats;
}

} // namespace nx::utils
