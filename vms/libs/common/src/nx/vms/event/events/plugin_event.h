#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/events/instant_event.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/events/events_fwd.h>

namespace nx::vms::event {

using namespace nx::vms::common;

class PluginEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit PluginEvent(
        qint64 timeStamp,
        const AnalyticsEngineResourcePtr& pluginInstance,
        const QString& caption,
        const QString& description,
        nx::vms::api::EventLevel level,
        const QnSecurityCamResourcePtr& camera);

    virtual bool checkEventParams(const EventParameters& params) const override;
    virtual EventParameters getRuntimeParams() const override;

private:
    const QnUuid m_resourceId;
    const QString m_caption;
    const QString m_description;
    /*const*/ EventMetaData m_metadata;
};

} // namespace nx::vms::event

Q_DECLARE_METATYPE(nx::vms::event::PluginEventPtr);
