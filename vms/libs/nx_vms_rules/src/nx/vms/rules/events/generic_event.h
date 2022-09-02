// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/icon.h>

#include "../data_macros.h"
#include "described_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API GenericEvent: public DescribedEvent
{
    using base_type = DescribedEvent;
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.generic")

    FIELD(QString, source, setSource)
    FIELD(QnUuidList, deviceIds, setDeviceIds)

public:
    GenericEvent() = default;
    GenericEvent(
        std::chrono::microseconds timestamp,
        State state,
        const QString& caption,
        const QString& description,
        const QString& source,
        const QnUuidList& deviceIds);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption() const;
    Icon icon() const;
};

} // namespace nx::vms::rules
