// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_single_device_field_validator.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/rules/camera_validation_policy.h>

#include "../action_builder_fields/target_single_device_field.h"
#include "../strings.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetSingleDeviceFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto targetSingleDeviceField = dynamic_cast<const TargetSingleDeviceField*>(field);
    if (!NX_ASSERT(targetSingleDeviceField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto deviceId = targetSingleDeviceField->id();
    const bool hasSelection = !deviceId.isNull() || targetSingleDeviceField->useSource();
    const auto targetSingleDeviceFieldProperties = targetSingleDeviceField->properties();

    if (hasSelection && !targetSingleDeviceFieldProperties.validationPolicy.isEmpty())
    {
        const auto device =
            context->resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);

        if (targetSingleDeviceFieldProperties.validationPolicy == kExecPtzValidationPolicy)
            return utils::cameraValidity<QnExecPtzPresetPolicy>(context, device);

        if (targetSingleDeviceFieldProperties.validationPolicy == kCameraFullScreenValidationPolicy)
            return utils::cameraValidity<QnFullscreenCameraPolicy>(context, device);

        return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
    }

    if (!targetSingleDeviceFieldProperties.allowEmptySelection && !hasSelection)
    {
        return {
            QValidator::State::Invalid,
            Strings::selectCamera(context, /*allowMultipleSelection*/ false)};
    }

    return {};
}

} // namespace nx::vms::rules
