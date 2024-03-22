// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/instant_event.h>
#include <nx/vms/event/events/events_fwd.h>

class QnUuid;

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API AnalyticsSdkObjectDetected: public InstantEvent
{
    using base_type = InstantEvent;

public:
    AnalyticsSdkObjectDetected(
        const QnResourcePtr& resource,
        const nx::common::metadata::ObjectMetadataPacketPtr& packet,
        const nx::common::metadata::ObjectMetadata& metadata);

    QnUuid engineId() const;
    const QString& objectTypeId() const;
    QnUuid objectTrackId() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

    static bool checkEventParams(
        nx::vms::common::SystemContext* systemContext,
        const EventParameters& params,
        const nx::common::metadata::ObjectMetadata& metadata);

    virtual QString getExternalUniqueKey() const override;

    const nx::common::metadata::Attributes& attributes() const;
    const std::optional<QString> attribute(const QString& attributeName) const;

private:
    const nx::common::metadata::ObjectMetadataPacketPtr m_packet;
    const nx::common::metadata::ObjectMetadata& m_metadata;
};

} // namespace event
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::event::AnalyticsSdkObjectDetectedPtr);
