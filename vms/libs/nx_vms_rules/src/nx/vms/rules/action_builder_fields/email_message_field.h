// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API EmailMessageField:
    public ActionBuilderField,
    public common::SystemContextAware
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.actions.fields.emailMessage")

public:
    explicit EmailMessageField(common::SystemContext* context);

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;
};

} // namespace nx::vms::rules
