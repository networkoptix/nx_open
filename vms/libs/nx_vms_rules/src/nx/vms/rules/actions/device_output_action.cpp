// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_output_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/output_port_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

QString DeviceOutputAction::uniqueKey() const
{
    return utils::makeName(BasicAction::uniqueKey(), m_outputPortId);
}

const ItemDescriptor& DeviceOutputAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceOutputAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Device Output")),
        .flags = ItemFlag::prolonged,
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(
                utils::kDeviceIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kCameraOutputValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<OutputPortField>(
                "outputPortId",
                NX_DYNAMIC_TRANSLATABLE(tr("Output ID"))),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                Strings::duration(),
                {},
                TimeFieldProperties{
                    .value = 1s,
                    .defaultValue = 1s,
                    .minimumValue = 1s}.toVariantMap()),
        },
        .resources = {{utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
