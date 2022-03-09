// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_factory.h"

#include <nx/vms/rules/action_fields/flag_field.h>
#include <nx/vms/rules/action_fields/optional_time_field.h>
#include <nx/vms/rules/action_fields/substitution.h>
#include <nx/vms/rules/action_fields/target_user_field.h>
#include <nx/vms/rules/action_fields/text_field.h>
#include <nx/vms/rules/action_fields/text_with_fields.h>
#include <nx/vms/rules/event_fields/analytics_event_type_field.h>
#include <nx/vms/rules/event_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_fields/customizable_text_field.h>
#include <nx/vms/rules/event_fields/expected_string_field.h>
#include <nx/vms/rules/event_fields/expected_uuid_field.h>
#include <nx/vms/rules/event_fields/int_field.h>
#include <nx/vms/rules/event_fields/keywords_field.h>
#include <nx/vms/rules/event_fields/source_camera_field.h>
#include <nx/vms/rules/event_fields/source_server_field.h>
#include <nx/vms/rules/event_fields/source_user_field.h>

#include "blank_picker_widget.h"
#include "oneline_text_picker_widget.h"
#include "multiline_text_picker_widget.h"
#include "number_picker_widget.h"
#include "state_picker_widget.h"
#include "source_picker_widget.h"

namespace nx::vms::client::desktop::rules {

using nx::vms::rules::fieldMetatype;

PickerWidget* PickerFactory::createWidget(
    const vms::rules::FieldDescriptor& descriptor,
    QWidget* parent)
{
    PickerWidget* pickerWidget{};

    if (descriptor.id == fieldMetatype<nx::vms::rules::CustomizableTextField>())
        pickerWidget = new CustomizableTextPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::ExpectedStringField>())
        pickerWidget = new ExpectedStringPicker(parent);
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
        pickerWidget = new UserPicker(parent);
    else if (descriptor.id == fieldMetatype<nx::vms::rules::IntField>())
        pickerWidget = new NumberPickerWidget<nx::vms::rules::IntField>(parent);
    else
        pickerWidget = new BlankPickerWidget(parent);

    pickerWidget->setDescriptor(descriptor);

    return pickerWidget;
}

} // namespace nx::vms::client::desktop::rules
