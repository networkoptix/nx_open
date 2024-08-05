// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpinBox>

#include <nx/vms/rules/utils/field.h>

#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class NumberPicker: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    NumberPicker(F* field, SystemContext* context, ParamsWidget* parent):
        base(field, context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_spinBox = new QSpinBox;
        contentLayout->addWidget(m_spinBox);
        contentLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_spinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &NumberPicker<F>::onValueChanged);
    }

protected:
    BASE_COMMON_USINGS

    QSpinBox* m_spinBox{nullptr};

    void updateUi() override
    {
        PlainFieldPickerWidget<F>::updateUi();

        QSignalBlocker blocker{m_spinBox};
        m_spinBox->setValue(m_field->value());
    }

    void onValueChanged(int value)
    {
        m_field->setValue(value);

        setEdited();
    }
};

} // namespace nx::vms::client::desktop::rules
