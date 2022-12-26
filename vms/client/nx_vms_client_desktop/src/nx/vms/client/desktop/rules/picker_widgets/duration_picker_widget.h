// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>

#include <ui/widgets/business/time_duration_widget.h>

#include "picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represended as a time period. */
template<typename F>
class DurationPickerWidget: public FieldPickerWidget<F>
{
public:
    DurationPickerWidget(SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_timeDurationWidget = new TimeDurationWidget;
        m_timeDurationWidget->setSizePolicy(
            QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
        m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
        m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

        contentLayout->addWidget(m_timeDurationWidget);

        m_contentWidget->setLayout(contentLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    TimeDurationWidget* m_timeDurationWidget;

    virtual void onDescriptorSet() override
    {
        m_label->setText(m_fieldDescriptor->displayName);
    }

    virtual void onFieldsSet() override
    {
        {
            QSignalBlocker blocker{m_timeDurationWidget};
            m_timeDurationWidget->setValue(m_field->value().count());
        }

        connect(m_timeDurationWidget,
            &TimeDurationWidget::valueChanged,
            this,
            &DurationPickerWidget::onValueChanged,
            Qt::UniqueConnection);
    }

    void onValueChanged()
    {
        m_field->setValue(std::chrono::seconds(m_timeDurationWidget->value()));
    }
};

} // namespace nx::vms::client::desktop::rules
