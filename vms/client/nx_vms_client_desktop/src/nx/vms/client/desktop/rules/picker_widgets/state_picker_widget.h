// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "ui_state_picker_widget.h"

#include <nx/vms/rules/action_fields/flag_field.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class StatePickerWidget: public FieldPickerWidget<F>
{
public:
    explicit StatePickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent),
        ui(new Ui::StatePickerWidget)
    {
        ui->setupUi(this);
    }

    virtual void setReadOnly(bool value) override
    {
        ui->stateCheckBox->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::edited;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QScopedPointer<Ui::StatePickerWidget> ui;

    virtual void onDescriptorSet() override
    {
        ui->stateCheckBox->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldSet() override
    {
        {
            QSignalBlocker blocker{ui->stateCheckBox};
            ui->stateCheckBox->setChecked(field->value());
        }

        connect(ui->stateCheckBox, &QCheckBox::stateChanged, this,
            [this](int state)
            {
                field->setValue(state == Qt::Checked);
                emit edited();
            },
            Qt::UniqueConnection);
    }
};

} // namespace nx::vms::client::desktop::rules
