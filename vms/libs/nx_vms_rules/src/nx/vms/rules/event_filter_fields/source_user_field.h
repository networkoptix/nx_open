// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

constexpr auto kUserInputValidationPolicy = "userInput";

class NX_VMS_RULES_API SourceUserField:
    public ResourceFilterEventField,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.user")

public:
    SourceUserField(common::SystemContext* context, const FieldDescriptor* descriptor);

    bool match(const QVariant& value) const override;

    static QJsonObject openApiDescriptor(const QVariantMap& properties);
};

} // namespace nx::vms::rules
