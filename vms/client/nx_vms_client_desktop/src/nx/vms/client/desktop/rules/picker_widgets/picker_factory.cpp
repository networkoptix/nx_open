// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_factory.h"

#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>

#include "blank_picker_widget.h"
#include "dropdown_id_picker_widget.h"
#include "dropdown_text_picker_widget.h"
#include "duration_picker_widget.h"
#include "flags_picker_widget.h"
#include "multiline_text_picker_widget.h"
#include "number_picker_widget.h"
#include "nx/vms/client/desktop/rules/picker_widgets/volume_picker_widget.h"
#include "nx/vms/rules/action_builder_fields/volume_field.h"
#include "nx/vms/rules/actions/speak_action.h"
#include "oneline_text_picker_widget.h"
#include "source_picker_widget.h"
#include "state_picker_widget.h"

namespace nx::vms::client::desktop::rules {

using nx::vms::rules::fieldMetatype;

PickerWidget* PickerFactory::createWidget(
    const vms::rules::FieldDescriptor& descriptor,
    SystemContext* context,
    QWidget* parent)
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
        pickerWidget = new StatePickerWidget<nx::vms::rules::FlagField>(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceCameraField>())
        pickerWidget = new CameraPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceServerField>())
        pickerWidget = new ServerPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceUserField>())
        pickerWidget = new SourceUserPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::IntField>())
        pickerWidget = new NumberPickerWidget<nx::vms::rules::IntField>(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::ContentTypeField>())
        pickerWidget = new HttpContentTypePicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::HttpMethodField>())
        pickerWidget = new HttpMethodPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::PasswordField>())
        pickerWidget = new PasswordPicker(context, parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::OptionalTimeField>())
        pickerWidget = new DurationPickerWidget<nx::vms::rules::OptionalTimeField>(context, parent);
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
    else if (descriptor.id == fieldMetatype<vms::rules::VolumeField>())
        pickerWidget = new VolumePickerWidget(context, parent);
    else
        pickerWidget = new BlankPickerWidget(context, parent);

    pickerWidget->setDescriptor(descriptor);

    return pickerWidget;
}

} // namespace nx::vms::client::desktop::rules
