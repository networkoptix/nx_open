// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>

#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename D = QComboBox>
class DropdownTextPickerWidgetBase: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    DropdownTextPickerWidgetBase(SystemContext* context, CommonParamsWidget* parent):
        base(context, parent)
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
    BASE_COMMON_USINGS

    D* m_comboBox{nullptr};

    virtual void onCurrentIndexChanged() = 0;
};

} // namespace nx::vms::client::desktop::rules
