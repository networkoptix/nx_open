// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_factory.h"

#include <nx/vms/rules/action_fields/builtin_fields.h>
#include <nx/vms/rules/event_fields/builtin_fields.h>

#include "blank_picker_widget.h"
#include "dropdown_text_picker_widget.h"
#include "duration_picker_widget.h"
#include "multiline_text_picker_widget.h"
#include "number_picker_widget.h"
#include "oneline_text_picker_widget.h"
#include "source_picker_widget.h"
#include "state_picker_widget.h"

namespace nx::vms::client::desktop::rules {

using nx::vms::rules::fieldMetatype;

PickerWidget* PickerFactory::createWidget(
    const vms::rules::FieldDescriptor& descriptor,
    QWidget* parent)
{
    PickerWidget* pickerWidget{};

    if (descriptor.id == fieldMetatype<nx::vms::rules::CustomizableTextField>())
        pickerWidget = new CustomizableTextPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::KeywordsField>())
        pickerWidget = new KeywordsPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::ActionTextField>())
        pickerWidget = new ActionTextPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::EventTextField>())
        pickerWidget = new EventTextPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::TextWithFields>())
        pickerWidget = new TextWithFieldsPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::FlagField>())
        pickerWidget = new StatePickerWidget<nx::vms::rules::FlagField>(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceCameraField>())
        pickerWidget = new CameraPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceServerField>())
        pickerWidget = new ServerPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::SourceUserField>())
        pickerWidget = new SourceUserPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::IntField>())
        pickerWidget = new NumberPickerWidget<nx::vms::rules::IntField>(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::ContentTypeField>())
        pickerWidget = new HttpContentTypePicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::HttpMethodField>())
        pickerWidget = new HttpMethodPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::PasswordField>())
        pickerWidget = new PasswordPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::OptionalTimeField>())
        pickerWidget = new DurationPickerWidget<nx::vms::rules::OptionalTimeField>(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::TargetUserField>())
        pickerWidget = new TargetUserPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::InputPortField>())
        pickerWidget = new InputPortPicker(parent);
    else
        pickerWidget = new BlankPickerWidget(parent);

    pickerWidget->setDescriptor(descriptor);

    return pickerWidget;
}

} // namespace nx::vms::client::desktop::rules
