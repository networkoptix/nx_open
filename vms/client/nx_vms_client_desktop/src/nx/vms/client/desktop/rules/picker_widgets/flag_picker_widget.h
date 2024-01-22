// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>

#include "picker_widget_utils.h"
#include "plain_picker_widget.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a boolean value. */
template<typename F>
class FlagPicker: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    FlagPicker(SystemContext* context, ParamsWidget* parent):
        base(context, parent)
    {
        auto contentLayout = new QHBoxLayout;
        contentLayout->setContentsMargins(0, 4, 0, 4);
        contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        contentLayout->addWidget(new QWidget);

        m_checkBox = new QCheckBox;
        m_checkBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        contentLayout->addWidget(m_checkBox);

        contentLayout->setStretch(0, 1);
        contentLayout->setStretch(1, 5);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &FlagPicker<F>::onStateChanged);
    }

private:
    BASE_COMMON_USINGS

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
