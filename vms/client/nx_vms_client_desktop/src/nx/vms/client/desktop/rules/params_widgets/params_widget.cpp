// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params_widget.h"

#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_widget.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/utils/common.h>
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

void ParamsWidget::setRule(const std::shared_ptr<vms::rules::Rule>& rule)
{
    if (!NX_ASSERT(!m_rule, "Rule must be set only once"))
        return;

    m_rule = rule;

    onRuleSet();

    updateUi();
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

std::optional<rules::FieldDescriptor> ParamsWidget::fieldDescriptor(const QString& fieldName) const
{
    return vms::rules::utils::fieldByName(fieldName, descriptor().value());
}

void ParamsWidget::setupLineEditsPlaceholderColor()
{
    const auto lineEdits = findChildren<QLineEdit*>();
    const auto midlightColor = QPalette().color(QPalette::Midlight);

    for (auto lineEdit: lineEdits)
        setPaletteColor(lineEdit, QPalette::PlaceholderText, midlightColor);
}

} // namespace nx::vms::client::desktop::rules
