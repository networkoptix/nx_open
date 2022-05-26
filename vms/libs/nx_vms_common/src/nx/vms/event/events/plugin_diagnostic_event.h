// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/events/instant_event.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/events/events_fwd.h>

namespace nx::vms::event {

using namespace nx::vms::common;

class NX_VMS_COMMON_API PluginDiagnosticEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit PluginDiagnosticEvent(
        qint64 timeStamp,
        const QnUuid& engineResourceId,
        const QString& caption,
        const QString& description,
        nx::vms::api::EventLevel level,
        const QnVirtualCameraResourcePtr& device);

    virtual bool checkEventParams(const EventParameters& params) const override;
    virtual EventParameters getRuntimeParams() const override;

    const QString& caption() const { return m_caption; }
    const QString& description() const { return m_description; }
    QnUuid engineId() const { return m_resourceId; }
    QnUuid deviceId() const
    {
        if (!m_metadata.cameraRefs.empty())
            return QnUuid(m_metadata.cameraRefs.front());

        return {};
    }

    nx::vms::api::EventLevel level() const { return m_metadata.level; }

private:
    const QnUuid m_resourceId;
    const QString m_caption;
    const QString m_description;
    /*const*/ EventMetaData m_metadata;
};

} // namespace nx::vms::event

Q_DECLARE_METATYPE(nx::vms::event::PluginDiagnosticEventPtr);
