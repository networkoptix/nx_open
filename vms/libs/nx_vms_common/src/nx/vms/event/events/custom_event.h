// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "prolonged_event.h"

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API CustomEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    explicit CustomEvent(
        const nx::vms::event::EventParameters& parameters,
        nx::vms::api::EventState eventState,
        const QnResourcePtr& resource);

    virtual bool checkEventParams(const EventParameters& params) const override;
    virtual EventParameters getRuntimeParams() const override;

private:
    const QString m_resourceName;
    const QString m_caption;
    const QString m_description;
    const EventMetaData m_metadata;
};

} // namespace event
} // namespace vms
} // namespace nx
