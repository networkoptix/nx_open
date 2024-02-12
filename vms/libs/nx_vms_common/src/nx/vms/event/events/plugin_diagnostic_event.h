// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/instant_event.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API PluginDiagnosticEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit PluginDiagnosticEvent(
        qint64 timeStamp,
        const nx::Uuid& engineResourceId,
        const QString& caption,
        const QString& description,
        nx::vms::api::EventLevel level,
        const QnVirtualCameraResourcePtr& device);

    virtual bool checkEventParams(const EventParameters& params) const override;
    virtual EventParameters getRuntimeParams() const override;

    const QString& caption() const { return m_caption; }
    const QString& description() const { return m_description; }
    nx::Uuid engineId() const { return m_engineResourceId; }
    nx::Uuid deviceId() const
    {
        if (!m_metadata.cameraRefs.empty())
            return nx::Uuid(m_metadata.cameraRefs.front());

        return {};
    }

    nx::vms::api::EventLevel level() const { return m_metadata.level; }

private:
    const nx::Uuid m_engineResourceId;
    const QString m_caption;
    const QString m_description;
    /*const*/ EventMetaData m_metadata;
};

} // namespace nx::vms::event
