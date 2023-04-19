// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../rules_fwd.h"

namespace nx::vms::api::rules {

struct ActionBuilder;
struct EventFilter;
struct Field;
struct Rule;

} // namespace nx::vms::api::rules


namespace nx::vms::rules {

namespace api = ::nx::vms::api::rules;

NX_VMS_RULES_API api::Rule serialize(const Rule* rule);
NX_VMS_RULES_API api::EventFilter serialize(const EventFilter* filter);
NX_VMS_RULES_API api::ActionBuilder serialize(const ActionBuilder *builder);

} // namespace nx::vms::rules
