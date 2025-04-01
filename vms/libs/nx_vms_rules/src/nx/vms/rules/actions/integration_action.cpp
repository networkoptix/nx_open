// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action.h"

#include "../action_builder_fields/integration_action_field.h"
#include "../action_builder_fields/integration_action_parameters_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"


namespace nx::vms::rules {

const ItemDescriptor& IntegrationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<IntegrationAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Integration Action")),
        .description = "Execute analytics Integration Action.",
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::currentServer,
        .fields = {
            makeFieldDescriptor<IntegrationActionField>(
                utils::kIntegrationActionFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Integration Action")),
                "Id of the Integration Action to execute.",
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<IntegrationActionParametersField>(
                utils::kIntegrationActionParametersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Integration Action Parameters")),
                "Parameters for the Integration Action to execute.",
                FieldProperties{.optional = false}.toVariantMap()),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
