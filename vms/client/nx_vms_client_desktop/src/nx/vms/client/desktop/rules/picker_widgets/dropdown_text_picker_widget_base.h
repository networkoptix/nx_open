// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename D = QComboBox>
class DropdownTextPickerWidgetBase: public FieldPickerWidget<F>
{
public:
    DropdownTextPickerWidgetBase(SystemContext* context, CommonParamsWidget* parent):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        m_comboBox = new D;
        m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_comboBox);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_comboBox,
            &QComboBox::currentTextChanged,
            this,
            &DropdownTextPickerWidgetBase<F, D>::onCurrentIndexChanged);
    }

protected:
    PICKER_WIDGET_COMMON_USINGS

    D* m_comboBox{};

    virtual void onCurrentIndexChanged() = 0;
};

} // namespace nx::vms::client::desktop::rules
