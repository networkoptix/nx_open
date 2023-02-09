// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_factory.h"

#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/actions/builtin_actions.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/type.h>

#include "analytics_object_attributes_picker_widget.h"
#include "blank_picker_widget.h"
#include "camera_picker_widget.h"
#include "customizable_icon_picker_widget.h"
#include "dropdown_id_picker_widget.h"
#include "duration_picker_widget.h"
#include "flag_picker_widget.h"
#include "flags_picker_widget.h"
#include "fps_picker_widget.h"
#include "http_parameters_picker_widget.h"
#include "input_port_picker_widget.h"
#include "keywords_picker_widget.h"
#include "multiline_text_picker_widget.h"
#include "number_picker_widget.h"
#include "oneline_text_picker_widget.h"
#include "server_picker_widget.h"
#include "single_target_device_picker_widget.h"
#include "source_user_picker_widget.h"
#include "state_picker_widget.h"
#include "stream_quality_picker_widget.h"
#include "target_layout_picker_widget.h"
#include "target_user_picker_widget.h"
#include "volume_picker_widget.h"

namespace nx::vms::client::desktop::rules {

namespace {

PickerWidget* createSourceCameraPicker(SystemContext* context, CommonParamsWidget* parent)
{
    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::AnalyticsEvent>())
        return new SourceCameraPicker<QnCameraAnalyticsPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::AnalyticsObjectEvent>())
        return new SourceCameraPicker<QnCameraAnalyticsPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::CameraInputEvent>())
        return new SourceCameraPicker<QnCameraInputPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::MotionEvent>())
        return new SourceCameraPicker<QnCameraMotionPolicy>(context, parent);

    return new SourceCameraPicker<QnAllowAnyCameraPolicy>(context, parent);;
}

PickerWidget* createSourceServerPicker(SystemContext* context, CommonParamsWidget* parent)
{
    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::PoeOverBudgetEvent>())
        return new SourceServerPicker<QnPoeOverBudgetPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::FanErrorEvent>())
        return new SourceServerPicker<QnFanErrorPolicy>(context, parent);

    NX_ASSERT(false, "Must not be here");
    return {};
}

PickerWidget* createTargetDevicePicker(SystemContext* context, CommonParamsWidget* parent)
{
    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::BookmarkAction>())
        return new TargetCameraPicker<QnBookmarkActionPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::DeviceOutputAction>())
        return new TargetCameraPicker<QnCameraOutputPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::DeviceRecordingAction>())
        return new TargetCameraPicker<QnCameraRecordingPolicy>(context, parent);

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::PlaySoundAction>()
        || parent->descriptor().id == vms::rules::utils::type<vms::rules::RepeatSoundAction>()
        || parent->descriptor().id == vms::rules::utils::type<vms::rules::SpeakAction>())
    {
        return new TargetCameraPicker<QnCameraAudioTransmitPolicy>(context, parent);
    }

    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::PtzPresetAction>())
        return new TargetCameraPicker<QnExecPtzPresetPolicy>(context, parent);

    NX_ASSERT(false, "Must not be here");
    return {};
}

PickerWidget* createSingleTargetCameraPicker(SystemContext* context, CommonParamsWidget* parent)
{
    if (parent->descriptor().id == vms::rules::utils::type<vms::rules::EnterFullscreenAction>())
        return new SingleTargetDevicePicker(context, parent);

    NX_ASSERT(false, "Must not be here.");
    return {};
}

} // namespace

using nx::vms::rules::fieldMetatype;

PickerWidget* PickerFactory::createWidget(
    const vms::rules::FieldDescriptor& descriptor,
    SystemContext* context,
    CommonParamsWidget* parent)
{
    PickerWidget* pickerWidget{};

    if (descriptor.id == fieldMetatype<nx::vms::rules::CustomizableTextField>())
        pickerWidget = new CustomizableTextPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::CustomizableIconField>())
        pickerWidget = new CustomizableIconPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::KeywordsField>())
        pickerWidget = new KeywordsPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::ActionTextField>())
        pickerWidget = new ActionTextPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::EventTextField>())
        pickerWidget = new EventTextPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::TextWithFields>())
        pickerWidget = new TextWithFieldsPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::FlagField>())
        pickerWidget = new FlagPickerWidget<nx::vms::rules::FlagField>(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceCameraField>())
        pickerWidget = createSourceCameraPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceServerField>())
        pickerWidget = createSourceServerPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceUserField>())
        pickerWidget = new SourceUserPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::StateField>())
        pickerWidget = new StatePicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::IntField>())
        pickerWidget = new NumberPickerWidget<nx::vms::rules::IntField>(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::ContentTypeField>())
        pickerWidget = new HttpContentTypePicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::HttpMethodField>())
        pickerWidget = new HttpMethodPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::PasswordField>())
        pickerWidget = new PasswordPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::OptionalTimeField>())
        pickerWidget = new OptionalDurationPickerWidget<nx::vms::rules::OptionalTimeField>(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::TimeField>())
        pickerWidget = new DurationPickerWidget<nx::vms::rules::TimeField>(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::TargetUserField>())
        pickerWidget = new TargetUserPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::InputPortField>())
        pickerWidget = new InputPortPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::AnalyticsEventLevelField>())
        pickerWidget = new FlagsPickerWidget<nx::vms::rules::AnalyticsEventLevelField>(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::AnalyticsEngineField>())
        pickerWidget = new AnalyticsEnginePicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::AnalyticsEventTypeField>())
        pickerWidget = new AnalyticsEventTypePicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::AnalyticsObjectTypeField>())
        pickerWidget = new AnalyticsObjectTypePicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::AnalyticsObjectAttributesField>())
        pickerWidget = new AnalyticsObjectAttributesPicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::VolumeField>())
        pickerWidget = new VolumePickerWidget(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::TargetDeviceField>())
        pickerWidget = createTargetDevicePicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::TargetSingleDeviceField>())
        pickerWidget = createSingleTargetCameraPicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::StreamQualityField>())
        pickerWidget = new StreamQualityPicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::FpsField>())
        pickerWidget = new FpsPicker(context, parent);
    else if (descriptor.id == fieldMetatype<vms::rules::TargetLayoutField>())
        pickerWidget = new TargetLayoutPicker(context, parent);
    else
        pickerWidget = new BlankPickerWidget(context, parent);

    pickerWidget->setDescriptor(descriptor);

    return pickerWidget;
}

} // namespace nx::vms::client::desktop::rules
