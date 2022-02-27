// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/vms/event/events/instant_event.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ConflictEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit ConflictEvent(
        const EventType eventType,
        const QnResourcePtr& resource,
        const qint64 timeStamp,
        const QString& caption = QString(),
        const QString& description = QString());

    virtual EventParameters getRuntimeParams() const override;

protected:
    QString m_caption;
    QString m_description;
};

} // namespace event
} // namespace vms
} // namespace nx
