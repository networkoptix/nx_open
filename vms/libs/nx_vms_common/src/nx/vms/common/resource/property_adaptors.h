// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/resource_property_adaptor.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::common {

// TODO: #amalov Remove in 5.2.
class NX_VMS_COMMON_API BusinessEventFilterResourcePropertyAdaptor:
    public QnLexicalResourcePropertyAdaptor<quint64>
{
    typedef QnLexicalResourcePropertyAdaptor<quint64> base_type;

public:
    BusinessEventFilterResourcePropertyAdaptor(QObject* parent = nullptr);

    bool isAllowed(nx::vms::api::EventType eventType) const;

    QList<nx::vms::api::EventType> watchedEvents() const;
    void setWatchedEvents(const QList<nx::vms::api::EventType>& events);
};

} // namespace nx::vms::common
