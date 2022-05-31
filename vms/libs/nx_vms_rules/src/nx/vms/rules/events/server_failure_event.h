// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "reasoned_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ServerFailureEvent: public ReasonedEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.serverFailure")

public:
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

    using ReasonedEvent::ReasonedEvent;

private:
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
