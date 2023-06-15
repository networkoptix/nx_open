// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional_duration_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/utils/field.h>

namespace nx::vms::client::desktop::rules {

OptionalDurationPicker::OptionalDurationPicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
     TitledFieldPickerWidget<vms::rules::OptionalTimeField>(context, parent)
{
    auto mainLayout = new QHBoxLayout;

    m_label = new QnElidedLabel;
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setElideMode(Qt::ElideRight);

    mainLayout->addWidget(m_label);

    m_timeDurationWidget = new TimeDurationWidget;
    mainLayout->addWidget(m_timeDurationWidget);

    mainLayout->setStretch(0, 1);
    mainLayout->setStretch(1, 5);

    m_contentWidget->setLayout(mainLayout);

    connect(
        m_timeDurationWidget,
        &TimeDurationWidget::valueChanged,
        this,
        [this]
        {
            theField()->setValue(std::chrono::seconds{m_timeDurationWidget->value()});
        });
}

void OptionalDurationPicker::onDescriptorSet()
{
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>::onDescriptorSet();

    if (m_fieldDescriptor->fieldName == vms::rules::utils::kDurationFieldName)
        m_label->setText(tr("Value"));
    else
        m_label->setText(tr("Timeout"));

    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

    auto maxIt = m_fieldDescriptor->properties.constFind("max");
    if (maxIt != m_fieldDescriptor->properties.constEnd())
        m_timeDurationWidget->setMaximum(maxIt->template value<std::chrono::seconds>().count());

    auto minIt = m_fieldDescriptor->properties.constFind("min");
    m_timeDurationWidget->setMinimum(minIt == m_fieldDescriptor->properties.constEnd()
        ? 0
        : minIt->template value<std::chrono::seconds>().count());
}

void OptionalDurationPicker::updateUi()
{
    auto field = theField();

    if (m_fieldDescriptor->fieldName == vms::rules::utils::kDurationFieldName)
    {
        const bool isProlonged =
            parentParamsWidget()->eventDescriptor()->flags.testFlag(vms::rules::ItemFlag::prolonged);

        if (!isProlonged && field->value() == vms::rules::OptionalTimeField::value_type::zero())
        {
            field->setValue(m_fieldDescriptor->properties.value("default").value<std::chrono::seconds>());
            return;
        }
    }

    {
        QSignalBlocker blocker{m_timeDurationWidget};
        m_timeDurationWidget->setValue(
            std::chrono::duration_cast<std::chrono::seconds>(field->value()).count());
    }

    setChecked(field->value() != vms::rules::OptionalTimeField::value_type::zero());
}

void OptionalDurationPicker::onEnabledChanged(bool isEnabled)
{
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>::onEnabledChanged(isEnabled);

    auto field = theField();
    if (isEnabled)
        field->setValue(m_fieldDescriptor->properties.value("default").value<std::chrono::seconds>());
    else
        field->setValue(vms::rules::OptionalTimeField::value_type::zero());
}

} // namespace nx::vms::client::desktop::rules
