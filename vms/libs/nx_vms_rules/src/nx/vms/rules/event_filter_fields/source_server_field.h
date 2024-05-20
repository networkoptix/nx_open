// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

constexpr auto kHasPoeManagementValidationPolicy = "hasPoeManagement";
constexpr auto kHasFanMonitoringValidationPolicy = "hasFanMonitoring";

class NX_VMS_RULES_API SourceServerField: public ResourceFilterEventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.server")

public:
    using ResourceFilterEventField::ResourceFilterEventField;
};

} // namespace nx::vms::rules
