// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional_duration_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/utils/field.h>
#include <ui/common/read_only.h>

namespace nx::vms::client::desktop::rules {

OptionalDurationPicker::OptionalDurationPicker(SystemContext* context, ParamsWidget* parent):
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

    if (m_fieldDescriptor->fieldName == vms::rules::utils::kIntervalFieldName)
        m_label->setText(tr("Once in"));
    else if (m_fieldDescriptor->fieldName == vms::rules::utils::kPlaybackTimeFieldName)
        m_label->setText(tr("For"));
    else
        m_label->setText(tr("Value"));

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
    const auto field = theField();

    if (m_fieldDescriptor->fieldName == vms::rules::utils::kIntervalFieldName)
    {
        const auto durationField =
            getActionField<vms::rules::OptionalTimeField>(vms::rules::utils::kDurationFieldName);
        if (durationField)
        {
            // If the action has duration field, 'Interval of action' picker must be visible only
            // when duration value is non zero.
            setVisible(durationField->value() != vms::rules::OptionalTimeField::value_type::zero());
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
