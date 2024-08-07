// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "speak_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../action_builder_fields/volume_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SpeakAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SpeakAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Speak")),
        .flags = {ItemFlag::instant},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TextWithFields>(utils::kTextFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Text"))),
            makeFieldDescriptor<TargetDeviceField>(
                utils::kDeviceIdsFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("At Device")),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kCameraAudioTransmissionValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("To users")),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .allowEmptySelection = true
                }.toVariantMap()),
            makeFieldDescriptor<VolumeField>(
                utils::kVolumeFieldName,
                Strings::volume()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
