// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buzzer_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_servers_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& BuzzerAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<BuzzerAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Buzzer")),
        .flags = ItemFlag::prolonged,
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TargetServersField>(
                utils::kServerIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kHasBuzzerValidationPolicy
                }.toVariantMap()),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                Strings::for_(),
                {},
                TimeFieldProperties{
                    .value = 1s,
                    .defaultValue = 1s,
                    .maximumValue = 24h,
                    .minimumValue = 1s}.toVariantMap()),
            utils::makeIntervalFieldDescriptor(
                NX_DYNAMIC_TRANSLATABLE(tr("Action Throttling"))),
        },
        .resources = {{utils::kServerIdsFieldName, {ResourceType::server, {}, {}, FieldFlag::target}}},
        .serverFlags = {api::ServerFlag::SF_HasBuzzer}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
