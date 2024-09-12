// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_device_field_validator.h"

#include <core/resource/device_dependent_strings.h>
#include <nx/vms/rules/camera_validation_policy.h>

#include "../action_builder_fields/target_devices_field.h"
#include "../engine.h"
#include "../event_filter.h"
#include "../rule.h"
#include "../strings.h"
#include "../utils/event.h"
#include "../utils/resource.h"
#include "../utils/validity.h"

namespace nx::vms::rules {

ValidationResult TargetDeviceFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto targetDeviceField = dynamic_cast<const TargetDevicesField*>(field);
    if (!NX_ASSERT(targetDeviceField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    const auto eventDescriptor =
        rule->engine()->eventDescriptor(rule->eventFilters().front()->eventType());
    if (!NX_ASSERT(eventDescriptor))
        return {QValidator::State::Invalid, {tr("Failed to get event descriptor")}};

    if (targetDeviceField->useSource() && !hasSourceCamera(eventDescriptor.value()))
        return {QValidator::State::Invalid, {tr("Event does not have source camera")}};

    const auto camerasSelection = targetDeviceField->selection();
    const auto targetDeviceFieldProperties = targetDeviceField->properties();
    const bool isValidSelection = !camerasSelection.ids.empty()
        || targetDeviceField->useSource()
        || targetDeviceFieldProperties.allowEmptySelection;

    if (!isValidSelection)
        return {QValidator::State::Invalid, Strings::selectCamera(context)};

    const auto cameras = utils::cameras(camerasSelection, context);
    if (!targetDeviceField->useSource() && cameras.empty() && !camerasSelection.ids.empty())
    {
        return {
            QValidator::State::Invalid,
            Strings::camerasWereRemoved(context, camerasSelection.ids.size())};
    }

    if (!targetDeviceFieldProperties.validationPolicy.isEmpty())
    {
        if (targetDeviceFieldProperties.validationPolicy == kBookmarkValidationPolicy)
            return utils::camerasValidity<QnBookmarkActionPolicy>(context, cameras);

        if (targetDeviceFieldProperties.validationPolicy == kCameraAudioTransmissionValidationPolicy)
            return utils::camerasValidity<QnCameraAudioTransmitPolicy>(context, cameras);

        if (targetDeviceFieldProperties.validationPolicy == kCameraOutputValidationPolicy)
            return utils::camerasValidity<QnCameraOutputPolicy>(context, cameras);

        if (targetDeviceFieldProperties.validationPolicy == kCameraRecordingValidationPolicy)
            return utils::camerasValidity<QnCameraRecordingPolicy>(context, cameras);

        return {QValidator::State::Invalid, Strings::unexpectedPolicy()};
    }

    return {};
}

} // namespace nx::vms::rules
