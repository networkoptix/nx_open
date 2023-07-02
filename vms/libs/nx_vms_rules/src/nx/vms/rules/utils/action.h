// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include "../rules_fwd.h"

namespace nx::vms::rules {

NX_VMS_RULES_API bool isProlonged(const Engine* engine, const ActionBuilder* builder);

NX_VMS_RULES_API bool hasTargetCamera(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetLayout(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetUser(const vms::rules::ItemDescriptor& actionDescriptor);

NX_VMS_RULES_API bool hasTargetServer(const vms::rules::ItemDescriptor& actionDescriptor);

// TODO: #amalov Remove when routing is ready.
/** Returns whether the user has permission to execute the action. */
NX_VMS_RULES_API bool checkUserPermissions(const QnUserResourcePtr& user, const ActionPtr& action);

} // namespace nx::vms::rules
