// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/action_builder_fields/flag_field.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a boolean value. */
template<typename F>
class FlagPicker: public FieldPickerWidget<F>
{
public:
    FlagPicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        m_checkBox = new QCheckBox;
        m_checkBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_checkBox);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &FlagPicker<F>::onStateChanged);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    QCheckBox* m_checkBox{nullptr};

    virtual void onDescriptorSet() override
    {
        m_label->setVisible(false);
        m_checkBox->setText(m_fieldDescriptor->displayName);
    }

    void onStateChanged(int state)
    {
        theField()->setValue(state == Qt::Checked);
    }

    void updateUi() override
    {
        QSignalBlocker blocker{m_checkBox};
        m_checkBox->setChecked(theField()->value());
    }
};

} // namespace nx::vms::client::desktop::rules
