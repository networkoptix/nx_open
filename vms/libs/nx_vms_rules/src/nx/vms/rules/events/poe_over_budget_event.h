// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PoeOverBudgetEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.poeOverBudget")

    FIELD(QnUuid, serverId, setServerId)

public:
    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString description() const;
};

} // namespace nx::vms::rules
