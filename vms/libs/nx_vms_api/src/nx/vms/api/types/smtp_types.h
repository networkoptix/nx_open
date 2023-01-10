// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

enum class ConnectionType
{
    unsecure = 0,
    ssl = 1,
    tls = 2,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ConnectionType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ConnectionType>;
    return visitor(
        Item{ConnectionType::unsecure, "Unsecure"},
        Item{ConnectionType::ssl, "Ssl"},
        Item{ConnectionType::tls, "Tls"}
    );
}

} // namespace nx::vms::api
