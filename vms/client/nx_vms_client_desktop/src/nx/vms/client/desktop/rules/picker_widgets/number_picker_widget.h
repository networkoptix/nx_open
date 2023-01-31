// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpinBox>

#include <nx/vms/rules/event_filter_fields/int_field.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class NumberPickerWidget: public SimpleFieldPickerWidget<F>
{
public:
    NumberPickerWidget(SystemContext* context, CommonParamsWidget* parent):
        SimpleFieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_SpinBox = new QSpinBox;
        contentLayout->addWidget(m_SpinBox);
        contentLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_SpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &NumberPickerWidget<F>::onValueChanged);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QSpinBox* m_SpinBox{};

    void updateValue() override
    {
        QSignalBlocker blocker{m_SpinBox};
        m_SpinBox->setValue(m_field->value());
    }

    void onValueChanged(int value)
    {
        m_field->setValue(value);
    }
};

} // namespace nx::vms::client::desktop::rules
