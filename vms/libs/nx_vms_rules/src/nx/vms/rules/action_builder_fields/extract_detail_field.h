// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../action_builder_field.h"
#include "../data_macros.h"

namespace nx::vms::rules {

/** Extract single value from event detail map. */
class NX_VMS_RULES_API ExtractDetailField:
    public ActionBuilderField,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.extractDetail")

    FIELD(QString, detailName, setDetailName);

public:
    ExtractDetailField(nx::vms::common::SystemContext* context, const FieldDescriptor* descriptor);

    QVariant build(const AggregatedEventPtr& event) const override;
    static QJsonObject openApiDescriptor();
};

} // namespace nx::vms::rules
