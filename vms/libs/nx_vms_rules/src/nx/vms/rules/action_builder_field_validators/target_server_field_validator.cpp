// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_server_field_validator.h"

#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/server_validation_policy.h>

#include "../action_builder_fields/target_server_field.h"
#include "../manifest.h"
#include "../strings.h"
#include "../utils/resource.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetServerFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto targetServerField = dynamic_cast<const TargetServerField*>(field);
    if (!NX_ASSERT(targetServerField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto serverId = targetServerField->value();
    if (serverId.isNull())
        return {};

    const auto server = context->resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server || !server->isOnline())
        return {QValidator::State::Invalid, tr("Select online server")};

    return {};
}

} // namespace nx::vms::rules
