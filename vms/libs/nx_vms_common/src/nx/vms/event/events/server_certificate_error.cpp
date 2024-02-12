// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_error.h"

#include <core/resource/media_server_resource.h>

namespace nx::vms::event {

ServerCertificateError::ServerCertificateError(
    QnMediaServerResourcePtr server, std::chrono::microseconds timestamp)
    :
    base_type(EventType::serverCertificateError, server, timestamp.count()),
    m_serverId(server->getId())
{
}

ServerCertificateError::ServerCertificateError(
    const nx::Uuid& serverId, std::chrono::microseconds timestamp)
    :
    base_type(EventType::serverCertificateError, /*resource*/ {}, timestamp.count()),
    m_serverId(serverId)
{
}

EventParameters ServerCertificateError::getRuntimeParams() const
{
    EventParameters eventParameters = base_type::getRuntimeParams();
    eventParameters.eventResourceId = m_serverId;
    return eventParameters;
}

} // namespace nx::vms::event
