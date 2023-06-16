// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/engine.h>

namespace nx::vms::client::desktop {

class EventModelData
{
public:
    EventModelData() = delete;

    EventModelData(const nx::vms::api::rules::EventLogRecord& record):
        m_record(record)
    {}

    const nx::vms::api::rules::EventLogRecord& record() const
    {
        return m_record;
    }

    nx::vms::rules::EventPtr event(SystemContext* context) const
    {
        if (!m_event)
            m_event = context->vmsRulesEngine()->buildEvent(m_record.eventData);

        NX_ASSERT(m_event);
        return m_event;
    }

    const QVariantMap& details(SystemContext* context) const
    {
        if (m_details.empty())
            m_details = event(context)->details(context);

        NX_ASSERT(!m_details.empty());
        return m_details;
    }

private:
    nx::vms::api::rules::EventLogRecord m_record;
    mutable nx::vms::rules::EventPtr m_event;
    mutable QVariantMap m_details;
};

} // namespace nx::vms::client::desktop
