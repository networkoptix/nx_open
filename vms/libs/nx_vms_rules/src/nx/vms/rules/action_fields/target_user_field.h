// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API TargetUserField: public ActionField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.targetUser")

    FIELD(bool, acceptAll, setAcceptAll)
    FIELD(QSet<QnUuid>, ids, setIds)

public:
    TargetUserField() = default;

    virtual QVariant build(const EventData& eventData) const override;
};

} // namespace nx::vms::rules
