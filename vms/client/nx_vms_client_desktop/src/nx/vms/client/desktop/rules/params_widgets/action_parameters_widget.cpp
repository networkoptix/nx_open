// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_parameters_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_factory.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_field.h>
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

void ActionParametersWidget::setEdited()
{
    NX_ASSERT(actionBuilder()->actionType() == m_actionType);

    for (auto picker: m_pickers)
        picker->setEdited();
}

void ActionParametersWidget::onRuleSet(bool isNewRule)
{
    m_actionType = actionBuilder()->actionType();

    auto layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_pickers.clear();

    if (auto statePicker = createStatePickerIfRequired())
    {
        if (!isNewRule)
            statePicker->setEdited();

        m_pickers.push_back(statePicker);
        layout->addWidget(statePicker);
    }

    const auto action = actionDescriptor();
    bool isPreviousPlain{true};
    for (const auto& fieldDescriptor: action->fields)
    {
        const auto field = actionBuilder()->fieldByName(fieldDescriptor.fieldName);
        const auto fieldProperties = field->properties();
        if (!fieldProperties.visible)
            continue;

        PickerWidget* picker = PickerFactory::createWidget(field, windowContext()->system(), this);
        if (picker == nullptr)
            continue;

        if (!isNewRule)
            picker->setEdited();

        const bool isCurrentPlain = !picker->hasSeparator();
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
    NX_ASSERT(actionBuilder()->actionType() == m_actionType);

    for (auto picker: m_pickers)
        picker->updateUi();
}

PickerWidget* ActionParametersWidget::createStatePickerIfRequired()
{
    if (vms::rules::utils::isInstantOnly(eventDescriptor().value()))
    {
        // If the event is only instant, thus user has only one option to choose, there is no sense
        // to show picker widget.
        return nullptr;
    }

    const auto stateField = eventFilter()->fieldByName(vms::rules::utils::kStateFieldName);
    if (!stateField)
    {
        // State field is not added to the event manifest.
        return nullptr;
    }

    if (vms::rules::utils::isProlongedOnly(actionDescriptor().value()))
    {
        // If the action is prolonged only, there are two possible options: the first one when the
        // action has 'duration' field, in that case state field will be controlled by the duration
        // picker widget. The second one when the action does not have 'duration' field (action
        // duration always equal to the event duration emitted the action), in that case user has
        // no option to chose state.
        return nullptr;
    }

    return PickerFactory::createWidget(stateField, windowContext()->system(), this);
}

} // namespace nx::vms::client::desktop::rules
