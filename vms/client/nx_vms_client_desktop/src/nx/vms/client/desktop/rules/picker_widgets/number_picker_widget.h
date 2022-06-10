// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpinBox>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_fields/int_field.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class NumberPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit NumberPickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent)
    {
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        mainLayout->addWidget(label);

        {
            auto spinboxLayout = new QHBoxLayout;

            valueSpinBox = new QSpinBox;
            spinboxLayout->addWidget(valueSpinBox);

            spinboxLayout->addItem(
                new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

            mainLayout->addLayout(spinboxLayout);
        }

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        setLayout(mainLayout);
    }

    virtual void setReadOnly(bool value) override
    {
        valueSpinBox->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::setLayout;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    QSpinBox* valueSpinBox{};

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldsSet() override
    {
        valueSpinBox->setValue(field->value());

        connect(valueSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &NumberPickerWidget<F>::onValueChanged,
            Qt::UniqueConnection);
    }

    void onValueChanged(int value)
    {
        field->setValue(value);
    }
};

} // namespace nx::vms::client::desktop::rules
