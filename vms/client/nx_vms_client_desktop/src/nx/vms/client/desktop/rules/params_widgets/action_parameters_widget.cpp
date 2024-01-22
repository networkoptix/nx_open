// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_parameters_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_factory.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/time_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/common.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/workbench/workbench_context.h>

#include "../dialog_details/dotted_line.h"
#include "../picker_widgets/plain_picker_widget.h"

namespace nx::vms::client::desktop::rules {

ActionParametersWidget::ActionParametersWidget(WindowContext* context, QWidget* parent):
    ParamsWidget(context, parent)
{
}

std::optional<vms::rules::ItemDescriptor> ActionParametersWidget::descriptor() const
{
    return actionDescriptor();
}

void ActionParametersWidget::onRuleSet()
{
    connect(
        actionBuilder(),
        &vms::rules::ActionBuilder::changed,
        this,
        &ActionParametersWidget::onActionFieldChanged);

    auto layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_pickers.clear();

    const auto event = eventDescriptor();
    const auto stateFieldDescriptor =
        vms::rules::utils::fieldByName(vms::rules::utils::kStateFieldName, event.value());
    const auto isEventInstantOnly = vms::rules::utils::isInstantOnly(event.value());

    const auto action = actionDescriptor();
    const auto hasDurationField = std::any_of(
        action->fields.cbegin(),
        action->fields.cend(),
        [](const auto& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kDurationFieldName;
        });

    // If the action has duration field, then state will be controlled by the duration picker widget.
    // If the event is only instant, thus user has only one option to choose, there is no sense to
    // show picker widget at all.
    if (stateFieldDescriptor && !isEventInstantOnly && !hasDurationField)
    {
        auto statePicker =
            PickerFactory::createWidget(stateFieldDescriptor.value(), windowContext(), this);
        m_pickers.push_back(statePicker);
        layout->addWidget(statePicker);
    }

    bool isPreviousPlain{true};
    for (const auto& fieldDescriptor: action->fields)
    {
        if (!fieldDescriptor.properties.value("visible", true).toBool())
            continue;

        PickerWidget* picker = PickerFactory::createWidget(fieldDescriptor, windowContext(), this);
        if (picker == nullptr)
            continue;

        const bool isCurrentPlain = dynamic_cast<PlainPickerWidget*>(picker) != nullptr;
        if (!isCurrentPlain || !isPreviousPlain)
        {
            layout->addSpacing(4);

            auto dottedLine = new DottedLineWidget;
            layout->addWidget(dottedLine);
        }

        isPreviousPlain = isCurrentPlain;

        m_pickers.push_back(picker);
        layout->addWidget(picker);
    }

    auto verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addItem(verticalSpacer);

    setLayout(layout);
}

void ActionParametersWidget::updateUi()
{
    for (auto picker: m_pickers)
        picker->updateUi();
}

void ActionParametersWidget::onActionFieldChanged(const QString& fieldName)
{
    if (fieldName == vms::rules::utils::kDurationFieldName)
        onActionDurationChanged();

    updateUi();
}

void ActionParametersWidget::onActionDurationChanged() const
{
    // During the rule editing by the user, some values might becomes inconsistent. The code below
    // is fixing such a values.
    // TODO: #mmalofeev find a better place for the code below.

    const auto durationField = actionBuilder()->fieldByName<vms::rules::OptionalTimeField>(
        vms::rules::utils::kDurationFieldName);
    if (!NX_ASSERT(durationField))
        return;

    const auto isEventInstantOnly = vms::rules::utils::isInstantOnly(eventDescriptor().value());
    if (isEventInstantOnly && durationField->value() == std::chrono::microseconds::zero())
    {
        // Zero duration does not have sense for instant only event.

        const auto fieldDescriptor = this->fieldDescriptor(vms::rules::utils::kDurationFieldName);
        if (!NX_ASSERT(fieldDescriptor))
            return;

        const auto defaultDuration =
            fieldDescriptor->properties.value("default").value<std::chrono::seconds>();

        QSignalBlocker blocker{durationField};
        if (NX_ASSERT(
            defaultDuration != std::chrono::seconds::zero(),
            "Default value for the '%1' field in the '%2' cannot be zero, fix the manifest",
            vms::rules::utils::kDurationFieldName,
            descriptor()->id))
        {
            durationField->setValue(defaultDuration);
        }
        else
        {
            durationField->setValue(std::chrono::seconds{1});
        }
    }

    const auto stateField = eventFilter()->fieldByType<vms::rules::StateField>();
    if (stateField)
    {
        // When a user is changed action duration from zero to non zero value or vice versa,
        // event state must also be corrected.

        QSignalBlocker blocker{stateField};
        if (durationField->value() == std::chrono::microseconds::zero())
        {
            // Action duration is requested to be equal the event duration. The only `State::none`
            // is accepted for such a case.
            stateField->setValue(api::rules::State::none);
            return;
        }

        if (stateField->value() == api::rules::State::none)
        {
            // Some non zero duration is selected. `State::none` must be changed to the other
            // appropriate state value.
            stateField->setValue(eventDescriptor()->flags.testFlag(vms::rules::ItemFlag::instant)
                ? api::rules::State::instant
                : api::rules::State::started);
        }
    }

    // When action has non zero duration, pre and post recording (except bookmark action
    // pre-recording) fields must have zero value.
    const auto fixTimeField =
        [this, durationField](const QString& fieldName)
        {
            const auto timeField = actionBuilder()->fieldByName<vms::rules::TimeField>(fieldName);
            if (!timeField)
                return;

            const auto timeFieldDescriptor = fieldDescriptor(fieldName);
            if (timeFieldDescriptor->linkedFields.contains(vms::rules::utils::kDurationFieldName))
            {
                if (durationField->value() != std::chrono::microseconds::zero())
                {
                    QSignalBlocker blocker{timeField};
                    timeField->setValue(std::chrono::microseconds::zero());
                }
            }
        };

    fixTimeField(vms::rules::utils::kRecordBeforeFieldName);
    fixTimeField(vms::rules::utils::kRecordAfterFieldName);
}

} // namespace nx::vms::client::desktop::rules
