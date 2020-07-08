#pragma once

#include <cstdint>

#include <nx/utils/url.h>

#include <QtCore/QString>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

template <typename Container, typename Key>
decltype(auto) at(Container&& container, const Key& key)
{
    try
    {
        return std::forward<Container>(container).at(key);
    }
    catch (const std::out_of_range&)
    {
        throw Exception("No element at key %1", key);
    }
}

nx::utils::Url withoutUserInfo(nx::utils::Url url);

std::int64_t parseIsoTimestamp(const QString& isoTimestamp);

} // namespace nx::vms_server_plugins::analytics::vivotek
