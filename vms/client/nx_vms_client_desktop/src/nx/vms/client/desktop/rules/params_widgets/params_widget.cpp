// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "params_widget.h"

#include <QtWidgets/QLineEdit>

#include <nx/vms/client/desktop/rules/picker_widgets/picker_widget.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;
using ItemDescriptor = vms::rules::ItemDescriptor;

ParamsWidget::ParamsWidget(QWidget* parent):
    QWidget(parent)
{
}

void ParamsWidget::setDescriptor(const ItemDescriptor& itemDescriptor)
{
    descriptor = itemDescriptor;

    onDescriptorSet();

    // At the moment all pickers must be instantiated.
    watchOnDataEdited();
    setupLineEditsPlaceholderColor();
}

void ParamsWidget::setInstant(bool /*value*/)
{
    // Some widgets not depends on state.
}

void ParamsWidget::setReadOnly(bool value)
{
    for (const auto& picker: findChildren<PickerWidget*>())
        picker->setReadOnly(value);
}

void ParamsWidget::onDescriptorSet()
{
}

void ParamsWidget::setFields(const QHash<QString, Field*>& fields)
{
    for (const auto& picker: findChildren<PickerWidget*>())
    {
        if (!picker->hasDescriptor())
            continue;

        auto pickerDescriptor = picker->descriptor();
        if (!fields.contains(pickerDescriptor->fieldName))
            continue;

        auto field = fields.value(pickerDescriptor->fieldName);
        picker->setField(fields[pickerDescriptor->fieldName]);
    }
}

std::optional<rules::FieldDescriptor> ParamsWidget::fieldDescriptor(const QString& fieldName)
{
    for (auto field = descriptor.fields.cbegin(); field != descriptor.fields.cend(); ++field)
    {
        if (field->fieldName == fieldName)
            return *field;
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

void ParamsWidget::watchOnDataEdited()
{
    for (const auto& picker: findChildren<PickerWidget*>())
        connect(picker, &PickerWidget::edited, this, &ParamsWidget::edited);
}

} // namespace nx::vms::client::desktop::rules
