#pragma once

#include <optional>

#include <QtCore/QString>

namespace nx::vms::server::resource {

struct MulticastParameters
{
    std::optional<std::string> address;
    std::optional<int> port;
    std::optional<int> ttl;

    QString toString() const;
};

} // namespace nx::vms::server::resource
