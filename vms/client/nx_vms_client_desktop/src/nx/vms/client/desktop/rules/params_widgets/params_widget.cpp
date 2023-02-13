// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params_widget.h"

#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_widget.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;
using ItemDescriptor = vms::rules::ItemDescriptor;

ParamsWidget::ParamsWidget(QnWorkbenchContext* context, QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(context)
{
}

void ParamsWidget::setDescriptor(const ItemDescriptor& value)
{
    m_itemDescriptor = value;

    onDescriptorSet();

    // At the moment all pickers must be instantiated.
    setupLineEditsPlaceholderColor();
}

const vms::rules::ItemDescriptor& ParamsWidget::descriptor() const
{
    return m_itemDescriptor;
}

void ParamsWidget::setRule(const std::shared_ptr<SimplifiedRule>& rule)
{
    m_rule = rule;
    updateUi();
}

std::optional<vms::rules::ItemDescriptor> ParamsWidget::actionDescriptor() const
{
    return m_rule->actionDescriptor();
}

vms::rules::ActionBuilder* ParamsWidget::actionBuilder() const
{
    return m_rule->actionBuilder();
}

std::optional<vms::rules::ItemDescriptor> ParamsWidget::eventDescriptor() const
{
    return m_rule->actionDescriptor();
}

vms::rules::EventFilter* ParamsWidget::eventFilter() const
{
    return m_rule->eventFilter();
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
    for (const auto& field: m_itemDescriptor.fields)
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
