// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_duration_picker_widget.h"

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/common/read_only.h>

namespace nx::vms::client::desktop::rules {

namespace {

constexpr auto kDurationOfEventIndex = 0;
constexpr auto kFixedDurationIndex = 1;

} // namespace

ActionDurationPickerWidget::ActionDurationPickerWidget(
    SystemContext* context,
    ParamsWidget* parent,
    const vms::rules::ItemDescriptor& eventDescriptor)
    :
    FieldPickerWidget<vms::rules::OptionalTimeField, PickerWidget>(context, parent)
{
    auto mainLayout = new QVBoxLayout{this};
    mainLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.height());
    mainLayout->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin,
        4,
        style::Metrics::kDefaultTopLevelMargin,
        4);

    {
        auto durationTypeLayout = new QHBoxLayout;
        durationTypeLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        durationTypeLayout->addWidget(new QWidget); //< Title is not required here.

        m_durationTypeComboBox = new QComboBox;
        m_durationTypeComboBox->addItem(tr("For the duration of event"));
        m_durationTypeComboBox->addItem(tr("Of fixed duration"));

        m_durationTypeComboBox->setSizePolicy(
            QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));

        durationTypeLayout->addWidget(m_durationTypeComboBox);

        durationTypeLayout->setStretch(0, 1);
        durationTypeLayout->setStretch(1, 5);

        mainLayout->addLayout(durationTypeLayout);
    }

    {
        m_contentWidget = new QWidget;
        auto contentLayout = new QHBoxLayout{m_contentWidget};
        contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

        const auto hasStateField = std::any_of(
            eventDescriptor.fields.cbegin(),
            eventDescriptor.fields.cend(),
            [](const auto& fieldDescriptor)
            {
                return fieldDescriptor.fieldName == vms::rules::utils::kStateFieldName;
            });

        if (hasStateField)
        {
            auto actionStartLabel = new QnElidedLabel;
            actionStartLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            actionStartLabel->setElideMode(Qt::ElideRight);
            actionStartLabel->setText(tr("Begin When"));

            contentLayout->addWidget(actionStartLabel);

            auto widget = new QWidget; //< Required for proper alignment.
            widget->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));
            auto layout = new QHBoxLayout{widget};
            layout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            m_actionStartComboBox = new QComboBox;
            if (eventDescriptor.flags.testFlag(vms::rules::ItemFlag::instant))
            {
                m_actionStartComboBox->addItem(
                    tr("Event Occurs"),
                    QVariant::fromValue(api::rules::State::instant));
            }

            if (eventDescriptor.flags.testFlag(vms::rules::ItemFlag::prolonged))
            {
                m_actionStartComboBox->addItem(
                    tr("Event Starts"),
                    QVariant::fromValue(api::rules::State::started));
                m_actionStartComboBox->addItem(
                    tr("Event Stops"),
                    QVariant::fromValue(api::rules::State::stopped));
            }

            layout->addWidget(m_actionStartComboBox);

            auto durationLabel = new QnElidedLabel;
            durationLabel->setSizePolicy(
                QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
            durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            durationLabel->setElideMode(Qt::ElideRight);
            durationLabel->setText(tr("Duration"));

            layout->addWidget(durationLabel);

            m_timeDurationWidget = new TimeDurationWidget;
            layout->addWidget(m_timeDurationWidget);

            contentLayout->addWidget(widget);
        }
        else
        {
            auto actionStartLabel = new QnElidedLabel;
            actionStartLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            actionStartLabel->setElideMode(Qt::ElideRight);
            actionStartLabel->setText(tr("Duration"));

            contentLayout->addWidget(actionStartLabel);

            m_timeDurationWidget = new TimeDurationWidget;
            contentLayout->addWidget(m_timeDurationWidget);
        }

        contentLayout->setStretch(0, 1);
        contentLayout->setStretch(1, 5);

        mainLayout->addWidget(m_contentWidget);
    }

    connect(
        m_durationTypeComboBox,
        &QComboBox::activated,
        this,
        &ActionDurationPickerWidget::onDurationTypeChanged);

    if (m_actionStartComboBox)
    {
        connect(
            m_actionStartComboBox,
            &QComboBox::activated,
            this,
            &ActionDurationPickerWidget::onStateChanged);
    }

    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
    m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);
    connect(
        m_timeDurationWidget,
        &TimeDurationWidget::valueChanged,
        this,
        &ActionDurationPickerWidget::onDurationValueChanged);
}

ActionDurationPickerWidget::~ActionDurationPickerWidget()
{
}

void ActionDurationPickerWidget::setReadOnly(bool value)
{
    m_durationTypeComboBox->setEnabled(value);
    m_actionStartComboBox->setEnabled(value);
    m_timeDurationWidget->setEnabled(value);
}

void ActionDurationPickerWidget::updateUi()
{
    if (m_actionStartComboBox)
    {
        const auto stateField =
            getEventField<vms::rules::StateField>(vms::rules::utils::kStateFieldName);
        m_actionStartComboBox->setCurrentIndex(
            m_actionStartComboBox->findData(QVariant::fromValue(stateField->value())));
    }

    const auto duration = theField()->value();
    const bool isZeroDuration = duration == std::chrono::microseconds::zero();

    m_durationTypeComboBox->setCurrentIndex(isZeroDuration
        ? kDurationOfEventIndex
        : kFixedDurationIndex);

    m_contentWidget->setVisible(!isZeroDuration);

    QSignalBlocker blocker{m_timeDurationWidget};
    m_timeDurationWidget->setValue(
        std::chrono::duration_cast<std::chrono::seconds>(duration).count());
}

void ActionDurationPickerWidget::onDescriptorSet()
{
}

void ActionDurationPickerWidget::onDurationTypeChanged()
{
    if (m_durationTypeComboBox->currentIndex() == kDurationOfEventIndex)
    {
        theField()->setValue(std::chrono::seconds::zero());
    }
    else
    {
        theField()->setValue(
            m_fieldDescriptor->properties.value("default").value<std::chrono::seconds>());
    }
}

void ActionDurationPickerWidget::onDurationValueChanged()
{
    theField()->setValue(std::chrono::seconds{m_timeDurationWidget->value()});
}

void ActionDurationPickerWidget::onStateChanged()
{
    const auto stateField = getEventField<vms::rules::StateField>(vms::rules::utils::kStateFieldName);
    stateField->setValue(m_actionStartComboBox->currentData().value<api::rules::State>());
}

} // namespace nx::vms::client::desktop::rules
