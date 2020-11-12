#pragma once

#include <cstdint>

#include <nx/utils/url.h>
#include <nx/vms_server_plugins/utils/exception.h>

#include <QtCore/QString>

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
        throw nx::vms_server_plugins::utils::Exception("No element at key %1", key);
    }
}

nx::utils::Url withoutUserInfo(nx::utils::Url url);

std::int64_t parseIsoTimestamp(const QString& isoTimestamp);

} // namespace nx::vms_server_plugins::analytics::vivotek
