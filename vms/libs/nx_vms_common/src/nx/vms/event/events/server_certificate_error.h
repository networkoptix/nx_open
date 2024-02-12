// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/instant_event.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API ServerCertificateError: public InstantEvent
{
    using base_type = InstantEvent;

public:
    ServerCertificateError(QnMediaServerResourcePtr server, std::chrono::microseconds timestamp);

    /** Use when there is no Server Resource for the serverId. */
    ServerCertificateError(const nx::Uuid& serverId, std::chrono::microseconds timestamp);

    virtual EventParameters getRuntimeParams() const override;

private:
    const nx::Uuid m_serverId;
};

} // namespace nx::vms::event
