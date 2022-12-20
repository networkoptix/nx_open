// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/action_builder_fields/flag_field.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class StatePickerWidget: public FieldPickerWidget<F>
{
public:
    StatePickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        m_checkBox = new QCheckBox;
        m_checkBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_checkBox);

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QCheckBox* m_checkBox{};

    virtual void onDescriptorSet() override
    {
        m_checkBox->setText(m_fieldDescriptor->displayName);
    }

    virtual void onFieldsSet() override
    {
        {
            QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(m_field->value());
        }

        connect(m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &StatePickerWidget<F>::onStateChanged,
            Qt::UniqueConnection);
    }

    void onStateChanged(int state)
    {
        m_field->setValue(state == Qt::Checked);
    }
};

} // namespace nx::vms::client::desktop::rules
