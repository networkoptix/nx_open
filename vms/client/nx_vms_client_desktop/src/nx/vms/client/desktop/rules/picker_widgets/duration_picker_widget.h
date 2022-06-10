// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <ui/widgets/business/time_duration_widget.h>
#include <ui/widgets/common/elided_label.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represended as a time period. */
template<typename F>
class DurationPickerWidget: public FieldPickerWidget<F>
{
public:
    explicit DurationPickerWidget(QWidget* parent = nullptr):
        FieldPickerWidget<F>(parent)
    {
        auto mainLayout = new QHBoxLayout;
        mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        label = new QnElidedLabel;
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        mainLayout->addWidget(label);

        timeDurationWidget = new TimeDurationWidget;
        timeDurationWidget->setSizePolicy(
            QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
        timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
        timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
        timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

        mainLayout->addWidget(timeDurationWidget);

        mainLayout->setStretch(0, 1);
        mainLayout->setStretch(1, 5);

        setLayout(mainLayout);
    }

    virtual void setReadOnly(bool value) override
    {
        timeDurationWidget->setEnabled(!value);
    }

private:
    using FieldPickerWidget<F>::connect;
    using FieldPickerWidget<F>::setLayout;
    using FieldPickerWidget<F>::fieldDescriptor;
    using FieldPickerWidget<F>::field;

    QnElidedLabel* label{};
    TimeDurationWidget* timeDurationWidget;

    virtual void onDescriptorSet() override
    {
        label->setText(fieldDescriptor->displayName);
    }

    virtual void onFieldsSet() override
    {
        {
            QSignalBlocker blocker{timeDurationWidget};
            timeDurationWidget->setValue(field->value().count());
        }

        connect(timeDurationWidget,
            &TimeDurationWidget::valueChanged,
            this,
            &DurationPickerWidget::onValueChanged,
            Qt::UniqueConnection);
    }

    void onValueChanged()
    {
        field->setValue(std::chrono::seconds(timeDurationWidget->value()));
    }
};

} // namespace nx::vms::client::desktop::rules
