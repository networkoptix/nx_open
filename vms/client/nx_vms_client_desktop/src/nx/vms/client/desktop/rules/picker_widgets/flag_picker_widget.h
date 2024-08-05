// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a boolean value. */
template<typename F>
class FlagPicker: public FieldPickerWidget<F, PickerWidget>
{
public:
    FlagPicker(F* field, SystemContext* context, ParamsWidget* parent):
        FieldPickerWidget<F, PickerWidget>{field, context, parent}
    {
        auto mainLayout = new QHBoxLayout{this};
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
        mainLayout->setContentsMargins(
            style::Metrics::kDefaultTopLevelMargin,
            4,
            style::Metrics::kDefaultTopLevelMargin,
            4);

        mainLayout->addWidget(new QWidget);

        m_checkBox = new QCheckBox;
        m_checkBox->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        m_checkBox->setText(field->descriptor()->displayName);
        mainLayout->addWidget(m_checkBox);

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &FlagPicker<F>::onStateChanged);
    }

    void setReadOnly(bool value) override
    {
        m_checkBox->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F, PickerWidget>::m_field;
    using FieldPickerWidget<F, PickerWidget>::connect;
    using FieldPickerWidget<F, PickerWidget>::setEdited;

    QCheckBox* m_checkBox{nullptr};

    void onStateChanged(int state)
    {
        m_field->setValue(state == Qt::Checked);

        setEdited();
    }

    void updateUi() override
    {
        FieldPickerWidget<F, PickerWidget>::updateUi();

        QSignalBlocker blocker{m_checkBox};
        m_checkBox->setChecked(m_field->value());
    }
};

} // namespace nx::vms::client::desktop::rules
