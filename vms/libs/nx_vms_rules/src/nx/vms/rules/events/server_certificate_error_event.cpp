// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_error_event.h"

#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ServerCertificateErrorEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerCertificateErrorEvent>(),
        .displayName = tr("Server Certificate Error"),
        .description = "",
    };
    return kDescriptor;
}

ServerCertificateErrorEvent::ServerCertificateErrorEvent(
    QnUuid serverId,
    std::chrono::microseconds timestamp)
    :
    base_type(timestamp),
    m_serverId(serverId)
{
}

} // namespace nx::vms::rules
