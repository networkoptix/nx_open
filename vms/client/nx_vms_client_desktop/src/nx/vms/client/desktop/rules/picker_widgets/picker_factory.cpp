// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_factory.h"

#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/camera_validation_policy.h>
#include <nx/vms/rules/event_filter_field.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/server_validation_policy.h>
#include <nx/vms/rules/user_validation_policy.h>
#include <nx/vms/rules/utils/type.h>

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
#include "http_headers_picker_widget.h"
#include "http_parameters_picker_widget.h"
#include "input_port_picker_widget.h"
#include "string_selection_picker_widget.h"
#include "multiline_text_picker_widget.h"
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

using namespace nx::vms::rules;

namespace {

template<class Picker>
PickerWidget* createPickerImpl(
    Field* field, SystemContext* context, ParamsWidget* parent)
{
    auto actualField = dynamic_cast<typename Picker::field_type*>(field);
    if (!NX_ASSERT(actualField))
        return {};

    auto result = new Picker{actualField, context, parent};

    result->setObjectName(field->descriptor()->fieldName); //< Required for auto tests.

    return result;
}

PickerWidget* createSourceCameraPicker(
    Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto sourceCameraField = dynamic_cast<SourceCameraField*>(field);
    if (!NX_ASSERT(sourceCameraField, "SourceCameraField is expected here"))
        return {};

    const auto validationPolicy = sourceCameraField->properties().validationPolicy;

    if (validationPolicy == kCameraAnalyticsEventsValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraAnalyticsEventsPolicy>>(field, context, parent);

    if (validationPolicy == kCameraAnalyticsObjectsValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraAnalyticsObjectsPolicy>>(field, context, parent);

    if (validationPolicy == kCameraInputValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraInputPolicy>>(field ,context, parent);

    if (validationPolicy == kCameraMotionValidationPolicy)
        return createPickerImpl<SourceCameraPicker<QnCameraMotionPolicy>>(field, context, parent);

    return createPickerImpl<SourceCameraPicker<QnAllowAnyCameraPolicy>>(field, context, parent);;
}

PickerWidget* createSourceServerPicker(
    Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto sourceServerField = dynamic_cast<SourceServerField*>(field);
    if (!NX_ASSERT(sourceServerField, "SourceServerField is expected here"))
        return {};

    const auto validationPolicy = sourceServerField->properties().validationPolicy;

    if (validationPolicy == kHasPoeManagementValidationPolicy)
        return createPickerImpl<SourceServerPicker<QnPoeOverBudgetPolicy>>(field, context, parent);

    if (validationPolicy == kHasFanMonitoringValidationPolicy)
        return createPickerImpl<SourceServerPicker<QnFanErrorPolicy>>(field, context, parent);

    NX_ASSERT(false, "Must not be here");
    return {};
}

PickerWidget* createTargetDevicePicker(
    Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto targetDeviceField = dynamic_cast<TargetDevicesField*>(field);
    if (!NX_ASSERT(targetDeviceField, "TargetDevicesField is expected here"))
        return {};

    const auto validationPolicy = targetDeviceField->properties().validationPolicy;

    if (validationPolicy == kBookmarkValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnBookmarkActionPolicy>>(field, context, parent);

    if (validationPolicy == kCameraOutputValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnCameraOutputPolicy>>(field, context, parent);

    if (validationPolicy == kCameraRecordingValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnCameraRecordingPolicy>>(field, context, parent);

    if (validationPolicy == kCameraAudioTransmissionValidationPolicy)
        return createPickerImpl<TargetCameraPicker<QnCameraAudioTransmitPolicy>>(field, context, parent);

    if (!targetDeviceField->properties().allowEmptySelection)
        return createPickerImpl<TargetCameraPicker<QnRequireCameraPolicy>>(field ,context, parent);

    return createPickerImpl<TargetCameraPicker<QnAllowAnyCameraPolicy>>(field, context, parent);
}

PickerWidget* createTargetServerPicker(
    Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto targetServerField = dynamic_cast<TargetServersField*>(field);
    if (!NX_ASSERT(targetServerField, "TargetServersField is expected here"))
        return {};

    if (targetServerField->properties().validationPolicy == kHasBuzzerValidationPolicy)
        return createPickerImpl<TargetServerPicker<QnBuzzerPolicy>>(field, context, parent);

    NX_ASSERT(false, "Must not be here");
    return {};
}

PickerWidget* createSingleTargetCameraPicker(
    Field* field, SystemContext* context, ParamsWidget* parent)
{
    const auto targetSingleDeviceField = dynamic_cast<TargetDeviceField*>(field);
    if (!NX_ASSERT(targetSingleDeviceField, "TargetDeviceField is expected here"))
        return {};

    const auto validationPolicy = targetSingleDeviceField->properties().validationPolicy;

    if (validationPolicy == kCameraFullScreenValidationPolicy)
        return createPickerImpl<SingleTargetDevicePicker<QnFullscreenCameraPolicy>>(field, context, parent);

    if (validationPolicy == kExecPtzValidationPolicy)
        return createPickerImpl<SingleTargetDevicePicker<QnExecPtzPresetPolicy>>(field, context, parent);

    NX_ASSERT(false, "Must not be here.");
    return {};
}

PickerWidget* createOptionalDurationPicker(
    Field* field,
    SystemContext* context,
    ParamsWidget* parent)
{
    if (field->descriptor()->fieldName == utils::kDurationFieldName)
    {
        const auto eventDurationType =
            getEventDurationType(context->vmsRulesEngine(), parent->eventFilter());
        if (eventDurationType == EventDurationType::instant)
            return createPickerImpl<DurationPicker<OptionalTimeField>>(field, context, parent);

        // Create picker widget which also controls event state.
        return createPickerImpl<ActionDurationPickerWidget>(field, context, parent);
    }

    return createPickerImpl<OptionalDurationPicker>(field, context, parent);
}

PickerWidget* createEventFieldWidget(
    Field* field,
    SystemContext* context,
    ParamsWidget* parent)
{
    const auto fieldId = field->descriptor()->type;

    if (fieldId == utils::type<AnalyticsEventLevelField>())
    {
        return createPickerImpl<FlagsPicker<AnalyticsEventLevelField>>(
            field, context, parent);
    }

    if (fieldId == utils::type<AnalyticsEventTypeField>())
        return createPickerImpl<AnalyticsEventTypePicker>(field, context, parent);

    if (fieldId == utils::type<AnalyticsEngineField>())
        return createPickerImpl<AnalyticsEnginePicker>(field, context, parent);

    if (fieldId == utils::type<AnalyticsAttributesField>())
        return createPickerImpl<AnalyticsObjectAttributesPicker>(field, context, parent);

    if (fieldId == utils::type<AnalyticsObjectTypeField>())
        return createPickerImpl<AnalyticsObjectTypePicker>(field, context, parent);

    if (fieldId == utils::type<CustomizableFlagField>())
    {
        return createPickerImpl<FlagPicker<CustomizableFlagField>>(
            field, context, parent);
    }

    if (fieldId == utils::type<CustomizableTextField>())
        return createPickerImpl<CustomizableTextPicker>(field, context, parent);

    if (fieldId == utils::type<CustomizableIconField>())
        return createPickerImpl<CustomizableIconPicker>(field, context, parent);

    if (fieldId == utils::type<EventFlagField>())
        return createPickerImpl<FlagPicker<EventFlagField>>(field, context, parent);

    if (fieldId == utils::type<EventTextField>())
        return createPickerImpl<EventTextPicker>(field, context, parent);

    if (fieldId == utils::type<InputPortField>())
        return createPickerImpl<InputPortPicker>(field, context, parent);

    if (fieldId == utils::type<IntField>())
        return createPickerImpl<NumberPicker<IntField>>(field, context, parent);

    if (fieldId == utils::type<ObjectLookupField>())
        return createPickerImpl<ObjectLookupPicker>(field, context, parent);

    if (fieldId == utils::type<SourceCameraField>())
        return createSourceCameraPicker(field, context, parent);

    if (fieldId == utils::type<SourceServerField>())
        return createSourceServerPicker(field, context, parent);

    if (fieldId == utils::type<SourceUserField>())
        return createPickerImpl<SourceUserPicker>(field, context, parent);

    if (fieldId == utils::type<StateField>())
        return createPickerImpl<StatePicker>(field, context, parent);

    if (fieldId == utils::type<TextLookupField>())
        return createPickerImpl<TextLookupPicker>(field, context, parent);

    return {};
}

PickerWidget* createActionFieldWidget(
    Field* field,
    SystemContext* context,
    ParamsWidget* parent)
{
    const auto fieldId = field->descriptor()->type;

    if (fieldId == utils::type<ActionFlagField>())
        return createPickerImpl<FlagPicker<ActionFlagField>>(field, context, parent);

    if (fieldId == utils::type<ActionTextField>())
        return createPickerImpl<ActionTextPicker>(field, context, parent);

    if (fieldId == utils::type<ContentTypeField>())
        return createPickerImpl<HttpContentTypePicker>(field, context, parent);

    if (fieldId == utils::type<FpsField>())
        return createPickerImpl<FpsPicker>(field, context, parent);

    if (fieldId == utils::type<HttpAuthField>())
        return createPickerImpl<HttpAuthPicker>(field, context, parent);

    if (fieldId == utils::type<HttpHeadersField>())
        return createPickerImpl<HttpHeadersPickerWidget>(field, context, parent);

    if (fieldId == utils::type<HttpMethodField>())
        return createPickerImpl<HttpMethodPicker>(field, context, parent);

    if (fieldId == utils::type<TargetLayoutField>())
        return createPickerImpl<SingleTargetLayoutPicker>(field, context, parent);

    if (fieldId == utils::type<TargetLayoutsField>())
        return createPickerImpl<TargetLayoutPicker>(field, context, parent);

    if (fieldId == utils::type<OptionalTimeField>())
        return createOptionalDurationPicker(field, context, parent);

    if (fieldId == utils::type<OutputPortField>())
        return createPickerImpl<OutputPortPicker>(field, context, parent);

    if (fieldId == utils::type<PasswordField>())
        return createPickerImpl<PasswordPicker>(field, context, parent);

    if (fieldId == utils::type<PtzPresetField>())
        return createPickerImpl<PtzPresetPicker>(field, context, parent);

    if (fieldId == utils::type<StreamQualityField>())
        return createPickerImpl<StreamQualityPicker>(field, context, parent);

    if (fieldId == utils::type<TargetDevicesField>())
        return createTargetDevicePicker(field, context, parent);

    if (fieldId == utils::type<TargetServersField>())
        return createTargetServerPicker(field, context, parent);

    if (fieldId == utils::type<TargetDeviceField>())
        return createSingleTargetCameraPicker(field, context, parent);

    if (fieldId == utils::type<SoundField>())
        return createPickerImpl<SoundPicker>(field, context, parent);

    if (fieldId == utils::type<TargetUsersField>())
        return createPickerImpl<TargetUserPicker>(field, context, parent);

    if (fieldId == utils::type<TextWithFields>())
        return createPickerImpl<TextWithFieldsPicker>(field, context, parent);

    if (fieldId == utils::type<TimeField>())
        return createPickerImpl<DurationPicker<TimeField>>(field, context, parent);

    if (fieldId == utils::type<VolumeField>())
        return createPickerImpl<VolumePicker>(field, context, parent);

    if (fieldId == utils::type<StringSelectionField>())
        return createPickerImpl<ListPicker>(field, context, parent);

    if (fieldId == utils::type<ActionIntField>())
        return createPickerImpl<NumberPicker<ActionIntField>>(field, context, parent);

    return {};
}

} // namespace

PickerWidget* PickerFactory::createWidget(
    Field* field,
    SystemContext* context,
    ParamsWidget* parent)
{
    const auto fieldId = field->descriptor()->type;

    if (dynamic_cast<ActionBuilderField*>(field))
        return createActionFieldWidget(field, context, parent);

    if (dynamic_cast<EventFilterField*>(field))
        return createEventFieldWidget(field, context, parent);

    return nullptr;
}

} // namespace nx::vms::client::desktop::rules
