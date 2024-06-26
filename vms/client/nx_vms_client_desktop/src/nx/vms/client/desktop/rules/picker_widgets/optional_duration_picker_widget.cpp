// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "optional_duration_picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/utils/field.h>
#include <ui/common/read_only.h>
#include <ui/widgets/common/elided_label.h>

namespace nx::vms::client::desktop::rules {

OptionalDurationPicker::OptionalDurationPicker(
    vms::rules::OptionalTimeField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>(field, context, parent)
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

    const auto fieldDescriptor = field->descriptor();
    if (fieldDescriptor->fieldName == vms::rules::utils::kIntervalFieldName)
        m_label->setText(tr("Once in"));
    else if (fieldDescriptor->fieldName == vms::rules::utils::kPlaybackTimeFieldName)
        m_label->setText(tr("For"));
    else
        m_label->setText(tr("Value"));

    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

    const auto fieldProperties = field->properties();
    m_timeDurationWidget->setMaximum(fieldProperties.maximumValue.count());
    m_timeDurationWidget->setMinimum(fieldProperties.minimumValue.count());

    connect(
        m_timeDurationWidget,
        &TimeDurationWidget::valueChanged,
        this,
        [this]
        {
            m_field->setValue(std::chrono::seconds{m_timeDurationWidget->value()});
        });
}

void OptionalDurationPicker::updateUi()
{
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>::updateUi();

    if (m_field->descriptor()->fieldName == vms::rules::utils::kIntervalFieldName)
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
            std::chrono::duration_cast<std::chrono::seconds>(m_field->value()).count());
    }

    setChecked(m_field->value() != vms::rules::OptionalTimeField::value_type::zero());
}

void OptionalDurationPicker::onEnabledChanged(bool isEnabled)
{
    TitledFieldPickerWidget<vms::rules::OptionalTimeField>::onEnabledChanged(isEnabled);

    if (isEnabled)
        m_field->setValue(m_field->properties().defaultValue);
    else
        m_field->setValue(vms::rules::OptionalTimeField::value_type::zero());
}

} // namespace nx::vms::client::desktop::rules
