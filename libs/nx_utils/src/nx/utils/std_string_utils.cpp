#include "std_string_utils.h"

#include <QtCore/QByteArray>

namespace nx::utils {

int stricmp(const std::string_view& left, const std::string_view& right)
{
    if (left.size() == right.size())
        return qstrnicmp(left.data(), right.data(), left.size());

    const auto result = qstrnicmp(left.data(), right.data(), std::min(left.size(), right.size()));
    if (result != 0)
        return result;

    return left.size() < right.size() ? -1 : 1;
}

} // namespace nx::utils
