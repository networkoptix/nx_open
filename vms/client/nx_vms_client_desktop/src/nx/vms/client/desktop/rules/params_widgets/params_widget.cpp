// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params_widget.h"

#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_widget.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;
using ItemDescriptor = vms::rules::ItemDescriptor;

ParamsWidget::ParamsWidget(SystemContext* context, QWidget* parent):
    QWidget(parent),
    SystemContextAware(context)
{
}

void ParamsWidget::setDescriptor(const ItemDescriptor& value)
{
    itemDescriptor = value;

    onDescriptorSet();

    // At the moment all pickers must be instantiated.
    setupLineEditsPlaceholderColor();
}

const vms::rules::ItemDescriptor& ParamsWidget::descriptor() const
{
    return itemDescriptor;
}

void ParamsWidget::setActionBuilder(vms::rules::ActionBuilder* actionBuilder)
{
    if (m_actionBuilder == actionBuilder)
        return;

    m_actionBuilder = actionBuilder;
    emit actionBuilderChanged();
}

vms::rules::ActionBuilder* ParamsWidget::actionBuilder() const
{
    return m_actionBuilder;
}

void ParamsWidget::setEventFilter(vms::rules::EventFilter* eventFilter)
{
    if (m_eventFilter == eventFilter)
        return;

    m_eventFilter = eventFilter;
    emit eventFilterChanged();
}

vms::rules::EventFilter* ParamsWidget::eventFilter() const
{
    return m_eventFilter;
}

void ParamsWidget::setReadOnly(bool value)
{
    for (const auto& picker: findChildren<PickerWidget*>())
        picker->setReadOnly(value);
}

void ParamsWidget::onDescriptorSet()
{
}

std::optional<rules::FieldDescriptor> ParamsWidget::fieldDescriptor(const QString& fieldName)
{
    for (const auto& field: itemDescriptor.fields)
    {
        if (field.fieldName == fieldName)
            return field;
    }

    return std::nullopt;
}

void ParamsWidget::setupLineEditsPlaceholderColor()
{
    const auto lineEdits = findChildren<QLineEdit*>();
    const auto midlightColor = QPalette().color(QPalette::Midlight);

    for (auto lineEdit: lineEdits)
        setPaletteColor(lineEdit, QPalette::PlaceholderText, midlightColor);
}

} // namespace nx::vms::client::desktop::rules
