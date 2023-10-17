// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params_widget.h"

#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_widget.h>
#include <nx/vms/rules/engine.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;
using ItemDescriptor = vms::rules::ItemDescriptor;

ParamsWidget::ParamsWidget(WindowContext* context, QWidget* parent):
    QWidget(parent),
    WindowContextAware(context)
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

void ParamsWidget::setRule(const std::shared_ptr<vms::rules::Rule>& rule)
{
    m_rule = rule;
    updateUi();

    // A queued connection is used below, because field value validation occurs while the field is
    // displayed. It may lead to the field value changing, which in turn tends to emit the
    // signal again before the field display function ends, which may lead to troubles.

    connect(
        m_rule->eventFilters().first(),
        &vms::rules::EventFilter::changed,
        this,
        &ParamsWidget::updateUi,
        Qt::QueuedConnection);

    connect(
        m_rule->actionBuilders().first(),
        &vms::rules::ActionBuilder::changed,
        this,
        &ParamsWidget::updateUi,
        Qt::QueuedConnection);
}

std::optional<vms::rules::ItemDescriptor> ParamsWidget::actionDescriptor() const
{
    return m_rule->engine()->actionDescriptor(m_rule->actionBuilders().first()->actionType());
}

vms::rules::ActionBuilder* ParamsWidget::actionBuilder() const
{
    return m_rule->actionBuilders().first();
}

std::optional<vms::rules::ItemDescriptor> ParamsWidget::eventDescriptor() const
{
    return m_rule->engine()->eventDescriptor(m_rule->eventFilters().first()->eventType());
}

vms::rules::EventFilter* ParamsWidget::eventFilter() const
{
    return m_rule->eventFilters().first();
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
