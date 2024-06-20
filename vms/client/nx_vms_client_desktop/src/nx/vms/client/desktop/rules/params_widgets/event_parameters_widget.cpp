// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameters_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_factory.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/workbench/workbench_context.h>

#include "../dialog_details/dotted_line.h"
#include "../picker_widgets/plain_picker_widget.h"

namespace nx::vms::client::desktop::rules {

EventParametersWidget::EventParametersWidget(WindowContext* context, QWidget* parent):
    ParamsWidget(context, parent)
{
}

std::optional<vms::rules::ItemDescriptor> EventParametersWidget::descriptor() const
{
    return eventDescriptor();
}

void EventParametersWidget::setEdited()
{
    for (auto picker: m_pickers)
        picker->setEdited();
}

void EventParametersWidget::onRuleSet(bool isNewRule)
{


    auto layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_pickers.clear();

    bool isPreviousPlain{true};
    const auto event = descriptor();
    for (const auto& fieldDescriptor: event->fields)
    {
        if (fieldDescriptor.fieldName == vms::rules::utils::kStateFieldName)
            continue; //< State field is displayed by the action editor.

        const auto field = eventFilter()->fieldByName(fieldDescriptor.fieldName);
        const auto fieldProperties = field->properties();
        if (!fieldProperties.visible)
            continue;

        PickerWidget* picker = PickerFactory::createWidget(field, windowContext()->system(), this);
        if (picker == nullptr)
            continue;

        if (!isNewRule)
            picker->setEdited();

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

    // Connection order is matter, must be called after all the pickers are created.
    connect(
        eventFilter(),
        &vms::rules::EventFilter::changed,
        this,
        &EventParametersWidget::onEventFieldChanged);
}

void EventParametersWidget::updateUi()
{
    for (auto picker: m_pickers)
        picker->updateUi();
}

void EventParametersWidget::onEventFieldChanged(const QString& /*fieldName*/)
{
    updateUi();
}

} // namespace nx::vms::client::desktop::rules
