// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "std_string_utils.h"

#include <QtCore/QByteArray>

namespace nx::utils {

int stricmp(const std::string_view& left, const std::string_view& right)
{
    if (left.size() == right.size())
        return qstrnicmp(left.data(), right.data(), (int) left.size());

    const auto result = qstrnicmp(left.data(), right.data(), (int) std::min(left.size(), right.size()));
    if (result != 0)
        return result;

    return left.size() < right.size() ? -1 : 1;
}

} // namespace nx::utils
