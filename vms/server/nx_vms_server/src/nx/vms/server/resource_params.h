#pragma once

#include <functional>
#include <variant>
#include <vector>

namespace nx::vms::server {

struct ResourceParams
{
    QString source;
    QByteArray data;

    // Returns the last correctly loaded ResourceParams from @sources that is chosen by @choose.
    static ResourceParams load(
        const std::vector<std::variant<QString, nx::utils::Url, ResourceParams>>& sources,
        const std::function<bool(const ResourceParams&)>& choose);
};

} // namespace nx::vms::server

