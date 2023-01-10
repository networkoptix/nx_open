// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

enum class RtpTransportType
{
    automatic,
    tcp,
    udp,
    multicast,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(RtpTransportType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<RtpTransportType>;
    return visitor(
        Item{RtpTransportType::automatic, "Auto"},
        Item{RtpTransportType::tcp, "TCP"},
        Item{RtpTransportType::udp, "UDP"},
        Item{RtpTransportType::multicast, "Multicast"}
    );
}

} // namespace nx::vms::api
