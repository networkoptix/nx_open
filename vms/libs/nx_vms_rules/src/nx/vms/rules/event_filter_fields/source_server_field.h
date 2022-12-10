// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SourceServerField: public ResourceFilterEventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.sourceServer")
};

} // namespace nx::vms::rules
