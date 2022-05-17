// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"
#include "described_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API GenericEvent: public DescribedEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.generic")

    FIELD(QString, source, setSource)

public:
    GenericEvent() = default;

    GenericEvent(
        std::chrono::microseconds timestamp,
        const QString& caption,
        const QString& description,
        const QString& source)
        :
        DescribedEvent(timestamp, caption, description),
        m_source(source)
    {
    }

    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
