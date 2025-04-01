// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action_field_validator.h"

#include <core/resource_management/resource_pool.h>
#include <nx/analytics/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include "../action_builder_fields/integration_action_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult IntegrationActionFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto integrationActionField = dynamic_cast<const IntegrationActionField*>(field);
    if (!NX_ASSERT(integrationActionField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const QString integrationAction = integrationActionField->value();

    if (integrationAction.isEmpty())
        return {QValidator::State::Invalid, Strings::selectIntegrationAction()};

    const auto engine = nx::analytics::findEngineByIntegrationActionId(
        integrationAction, context->resourcePool());
    if (!engine)
    {
        return {
            QValidator::State::Invalid,
            Strings::integrationNotFoundForIntegrationAction(integrationAction)};
    }

    return {};
}

} // namespace nx::vms::rules
