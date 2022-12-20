// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpinBox>

#include <nx/vms/rules/event_filter_fields/int_field.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class NumberPickerWidget: public FieldPickerWidget<F>
{
public:
    NumberPickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_valueSpinBox = new QSpinBox;
        contentLayout->addWidget(m_valueSpinBox);
        contentLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QSpinBox* m_valueSpinBox{};

    virtual void onFieldsSet() override
    {
        m_valueSpinBox->setValue(m_field->value());

        connect(m_valueSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &NumberPickerWidget<F>::onValueChanged,
            Qt::UniqueConnection);
    }

    void onValueChanged(int value)
    {
        m_field->setValue(value);
    }
};

} // namespace nx::vms::client::desktop::rules
