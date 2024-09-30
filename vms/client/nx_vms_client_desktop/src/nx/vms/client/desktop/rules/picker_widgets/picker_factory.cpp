// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_factory.h"

#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/camera_validation_policy.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/server_validation_policy.h>
#include <nx/vms/rules/user_validation_policy.h>

#include "action_duration_picker_widget.h"
#include "analytics_object_attributes_picker_widget.h"
#include "camera_picker_widget.h"
#include "customizable_icon_picker_widget.h"
#include "dropdown_id_picker_widget.h"
#include "duration_picker_widget.h"
#include "flag_picker_widget.h"
#include "flags_picker_widget.h"
#include "fps_picker_widget.h"
#include "http_auth_picker_widget.h"
#include "http_parameters_picker_widget.h"
#include "input_port_picker_widget.h"
#include "number_picker_widget.h"
#include "object_lookup_picker_widget.h"
#include "oneline_text_picker_widget.h"
#include "optional_duration_picker_widget.h"
#include "output_port_picker_widget.h"
#include "ptz_preset_picker_widget.h"
#include "server_picker_widget.h"
#include "single_target_device_picker_widget.h"
#include "single_target_layout_picker_widget.h"
#include "sound_picker_widget.h"
#include "source_user_picker_widget.h"
#include "state_picker_widget.h"
#include "stream_quality_picker_widget.h"
#include "target_layout_picker_widget.h"
#include "target_user_picker_widget.h"
#include "text_lookup_picker_widget.h"
#include "text_with_fields_picker_widget.h"
#include "volume_picker_widget.h"

namespace nx::vms::client::desktop::rules {

namespace {

template<class Picker>
PickerWidget* createPickerImpl(
    vms::rules::Field* field, SystemContext* context, ParamsWidget* parent)
{
    auto actualField = dynamic_cast<typename Picker::field_type*>(field);
    if (!NX_ASSERT(actualField))
        return {};

    auto result = new Picker{actualField, context, parent};

    result->setObjectName(field->descriptor()->fieldName); //< Required for auto tests.

    return result;
}

PickerWidget* createSourceCameraPicker(
    vms::rules::Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto sourceCameraField = dynamic_cast<vms::rules::SourceCameraField*>(field);
    if (!NX_ASSERT(sourceCameraField, "SourceCameraField is expected here"))
        return {};

    const auto validationPolicy = sourceCameraField->properties().validationPolicy;

    if (validationPolicy == vms::rules::kCameraAnalyticsValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraAnalyticsPolicy>>(field, context, parent);

    if (validationPolicy == vms::rules::kCameraInputValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraInputPolicy>>(field ,context, parent);

    if (validationPolicy == vms::rules::kCameraMotionValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraMotionPolicy>>(field, context, parent);

    return createPickerImpl<SourceCameraPicker<QnAllowAnyCameraPolicy>>(field, context, parent);;
}

PickerWidget* createSourceServerPicker(
    vms::rules::Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto sourceServerField = dynamic_cast<vms::rules::SourceServerField*>(field);
    if (!NX_ASSERT(sourceServerField, "SourceServerField is expected here"))
        return {};

    const auto validationPolicy = sourceServerField->properties().validationPolicy;

    if (validationPolicy == vms::rules::kHasPoeManagementValidationPolicy)
        return createPickerImpl<SourceServerPicker<QnPoeOverBudgetPolicy>>(field, context, parent);

    if (validationPolicy == vms::rules::kHasFanMonitoringValidationPolicy)
        return createPickerImpl<SourceServerPicker<QnFanErrorPolicy>>(field, context, parent);

    NX_ASSERT(false, "Must not be here");
    return {};
}

PickerWidget* createTargetDevicePicker(
    vms::rules::Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto targetDeviceField = dynamic_cast<vms::rules::TargetDevicesField*>(field);
    if (!NX_ASSERT(targetDeviceField, "TargetDevicesField is expected here"))
        return {};

    const auto validationPolicy = targetDeviceField->properties().validationPolicy;

    if (validationPolicy == vms::rules::kBookmarkValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnBookmarkActionPolicy>>(field, context, parent);

    if (validationPolicy == vms::rules::kCameraOutputValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnCameraOutputPolicy>>(field, context, parent);

    if (validationPolicy == vms::rules::kCameraRecordingValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnCameraRecordingPolicy>>(field, context, parent);

    if (validationPolicy == vms::rules::kCameraAudioTransmissionValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnCameraAudioTransmitPolicy>>(field, context, parent);

    if (!targetDeviceField->properties().allowEmptySelection)
        return createPickerImpl<TargetCameraPicker<QnRequireCameraPolicy>>(field ,context, parent);

    return createPickerImpl<TargetCameraPicker<QnAllowAnyCameraPolicy>>(field, context, parent);
}

PickerWidget* createTargetServerPicker(
    vms::rules::Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto targetServerField = dynamic_cast<vms::rules::TargetServersField*>(field);
    if (!NX_ASSERT(targetServerField, "TargetServersField is expected here"))
        return {};

    if (targetServerField->properties().validationPolicy == vms::rules::kHasBuzzerValidationPolicy)
        return createPickerImpl<TargetServerPicker<QnBuzzerPolicy>>(field, context, parent);

    NX_ASSERT(false, "Must not be here");
    return {};
}

PickerWidget* createSingleTargetCameraPicker(
    vms::rules::Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto targetSingleDeviceField = dynamic_cast<vms::rules::TargetDeviceField*>(field);
    if (!NX_ASSERT(targetSingleDeviceField, "TargetDeviceField is expected here"))
        return {};

    const auto validationPolicy = targetSingleDeviceField->properties().validationPolicy;

    if (validationPolicy == vms::rules::kCameraFullScreenValidationPolicy)
        return createPickerImpl<SingleTargetDevicePicker<QnFullscreenCameraPolicy>>(field, context, parent);

    if (validationPolicy == vms::rules::kExecPtzValidationPolicy)
        return createPickerImpl<SingleTargetDevicePicker<QnExecPtzPresetPolicy>>(field, context, parent);

    NX_ASSERT(false, "Must not be here.");
    return {};
}

PickerWidget* createOptionalDurationPicker(
    vms::rules::Field* field,
    SystemContext* context,
    ParamsWidget* parent)
{
    if (field->descriptor()->fieldName == vms::rules::utils::kDurationFieldName)
    {
        const auto eventDurationType =
            vms::rules::getEventDurationType(context->vmsRulesEngine(), parent->eventFilter());
        if (eventDurationType == vms::rules::EventDurationType::instant)
            return createPickerImpl<DurationPicker<vms::rules::OptionalTimeField>>(field, context, parent);

        // Create picker widget which also controls event state.
        return createPickerImpl<ActionDurationPickerWidget>(field, context, parent);
    }

    return createPickerImpl<OptionalDurationPicker>(field, context, parent);
}

} // namespace

using nx::vms::rules::fieldMetatype;

PickerWidget* PickerFactory::createWidget(
    vms::rules::Field* field,
    SystemContext* context,
    ParamsWidget* parent)
{
    const auto fieldId = field->descriptor()->id;

    // Event field based pickers.
    if (fieldId == fieldMetatype<vms::rules::AnalyticsEventLevelField>())
    {
        return createPickerImpl<FlagsPicker<vms::rules::AnalyticsEventLevelField>>(
            field, context, parent);
    }

    if (fieldId == fieldMetatype<vms::rules::AnalyticsEventTypeField>())
        return createPickerImpl<AnalyticsEventTypePicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::AnalyticsEngineField>())
        return createPickerImpl<AnalyticsEnginePicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::AnalyticsObjectAttributesField>())
        return createPickerImpl<AnalyticsObjectAttributesPicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::AnalyticsObjectTypeField>())
        return createPickerImpl<AnalyticsObjectTypePicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::CustomizableFlagField>())
    {
        return createPickerImpl<FlagPicker<vms::rules::CustomizableFlagField>>(
            field, context, parent);
    }

    if (fieldId == fieldMetatype<nx::vms::rules::CustomizableTextField>())
        return createPickerImpl<CustomizableTextPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::CustomizableIconField>())
        return createPickerImpl<CustomizableIconPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::EventFlagField>())
        return createPickerImpl<FlagPicker<vms::rules::EventFlagField>>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::EventTextField>())
        return createPickerImpl<EventTextPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::InputPortField>())
        return createPickerImpl<InputPortPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::IntField>())
        return createPickerImpl<NumberPicker<vms::rules::IntField>>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::ObjectLookupField>())
        return createPickerImpl<ObjectLookupPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::SourceCameraField>())
        return createSourceCameraPicker(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::SourceServerField>())
        return createSourceServerPicker(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::SourceUserField>())
        return createPickerImpl<SourceUserPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::StateField>())
        return createPickerImpl<StatePicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::TextLookupField>())
        return createPickerImpl<TextLookupPicker>(field, context, parent);

    // Action field based pickers.
    if (fieldId == fieldMetatype<nx::vms::rules::ActionFlagField>())
        return createPickerImpl<FlagPicker<nx::vms::rules::ActionFlagField>>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::ActionTextField>())
        return createPickerImpl<ActionTextPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::ContentTypeField>())
        return createPickerImpl<HttpContentTypePicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::FpsField>())
        return createPickerImpl<FpsPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::HttpMethodField>())
        return createPickerImpl<HttpMethodPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::HttpAuthField>())
        return createPickerImpl<HttpAuthPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::TargetLayoutField>())
        return createPickerImpl<SingleTargetLayoutPicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::TargetLayoutsField>())
        return createPickerImpl<TargetLayoutPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::OptionalTimeField>())
        return createOptionalDurationPicker(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::OutputPortField>())
        return createPickerImpl<OutputPortPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::PasswordField>())
        return createPickerImpl<PasswordPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::PtzPresetField>())
        return createPickerImpl<PtzPresetPicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::StreamQualityField>())
        return createPickerImpl<StreamQualityPicker>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::TargetDevicesField>())
        return createTargetDevicePicker(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::TargetServersField>())
        return createTargetServerPicker(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::TargetDeviceField>())
        return createSingleTargetCameraPicker(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::SoundField>())
        return createPickerImpl<SoundPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::TargetUsersField>())
        return createPickerImpl<TargetUserPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::TextWithFields>())
        return createPickerImpl<TextWithFieldsPicker>(field, context, parent);

    if (fieldId == fieldMetatype<nx::vms::rules::TimeField>())
        return createPickerImpl<DurationPicker<nx::vms::rules::TimeField>>(field, context, parent);

    if (fieldId == fieldMetatype<vms::rules::VolumeField>())
        return createPickerImpl<VolumePicker>(field, context, parent);

    return {};
}

} // namespace nx::vms::client::desktop::rules
