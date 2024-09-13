// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_duration_picker_widget.h"

#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/field.h>
#include <ui/common/read_only.h>
#include <ui/widgets/common/elided_label.h>

#include "common.h"

namespace nx::vms::client::desktop::rules {

namespace {

constexpr auto kDurationOfEventIndex = 0;
constexpr auto kFixedDurationIndex = 1;

} // namespace

ActionDurationPickerWidget::ActionDurationPickerWidget(
    vms::rules::OptionalTimeField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    FieldPickerWidget<vms::rules::OptionalTimeField, PickerWidget>(field, context, parent)
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

        const auto eventDescriptor = parentParamsWidget()->eventDescriptor();

        const auto hasStateField = std::any_of(
            eventDescriptor->fields.cbegin(),
            eventDescriptor->fields.cend(),
            [](const auto& fieldDescriptor)
            {
                return fieldDescriptor.fieldName == vms::rules::utils::kStateFieldName;
            });

        if (hasStateField)
        {
            auto contentLayout = new QVBoxLayout{m_contentWidget};
            contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            auto durationTypeLayout = new QHBoxLayout;
            durationTypeLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            auto actionStartLabel = new QnElidedLabel;
            actionStartLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            actionStartLabel->setElideMode(Qt::ElideRight);
            actionStartLabel->setText(vms::rules::Strings::beginWhen().value());

            durationTypeLayout->addWidget(actionStartLabel);

            m_actionStartComboBox = new StateComboBox;
            m_actionStartComboBox->setStateStringProvider(
                [](vms::rules::State state)
                {
                    switch(state)
                    {
                        case vms::rules::State::instant:
                            return tr("Event occurs");
                        case vms::rules::State::started:
                            return tr("Event starts");
                        case vms::rules::State::stopped:
                            return tr("Event stops");
                        default:
                            NX_ASSERT(false, "Unexpected state is provided");
                            return QString{};
                    }
                });

            durationTypeLayout->addWidget(m_actionStartComboBox);

            durationTypeLayout->setStretch(0, 1);
            durationTypeLayout->setStretch(1, 5);

            contentLayout->addLayout(durationTypeLayout);

            auto durationLayout = new QHBoxLayout;
            durationLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            auto durationLabel = new QnElidedLabel;
            durationLabel->setSizePolicy(
                QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
            durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            durationLabel->setElideMode(Qt::ElideRight);
            durationLabel->setText(tr("Duration"));

            durationLayout->addWidget(durationLabel);

            m_timeDurationWidget = new TimeDurationWidget;
            durationLayout->addWidget(m_timeDurationWidget);

            durationLayout->setStretch(0, 1);
            durationLayout->setStretch(1, 5);

            contentLayout->addLayout(durationLayout);
        }
        else
        {
            auto contentLayout = new QHBoxLayout{m_contentWidget};
            contentLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());

            auto actionStartLabel = new QnElidedLabel;
            actionStartLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            actionStartLabel->setElideMode(Qt::ElideRight);
            actionStartLabel->setText(tr("For"));

            contentLayout->addWidget(actionStartLabel);

            m_timeDurationWidget = new TimeDurationWidget;
            contentLayout->addWidget(m_timeDurationWidget);

            contentLayout->setStretch(0, 1);
            contentLayout->setStretch(1, 5);
        }

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

    const auto fieldProperties = field->properties();
    m_timeDurationWidget->setMaximum(fieldProperties.maximumValue.count());
    m_timeDurationWidget->setMinimum(fieldProperties.minimumValue.count());

    m_timeDurationWidget->valueSpinBox()->setFixedWidth(kDurationValueSpinBoxWidth);

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
        const auto eventDurationType = vms::rules::getEventDurationType(
            systemContext()->vmsRulesEngine(), parentParamsWidget()->eventFilter());
        m_actionStartComboBox->update(eventDurationType);

        const auto stateField =
            getEventField<vms::rules::StateField>(vms::rules::utils::kStateFieldName);
        m_actionStartComboBox->setCurrentIndex(
            m_actionStartComboBox->findData(QVariant::fromValue(stateField->value())));
    }

    const auto duration = m_field->value();
    const bool isZeroDuration = duration == std::chrono::microseconds::zero();

    m_durationTypeComboBox->setCurrentIndex(isZeroDuration
        ? kDurationOfEventIndex
        : kFixedDurationIndex);

    m_contentWidget->setVisible(!isZeroDuration);

    QSignalBlocker blocker{m_timeDurationWidget};
    m_timeDurationWidget->setValue(
        std::chrono::duration_cast<std::chrono::seconds>(duration).count());
}

void ActionDurationPickerWidget::onDurationTypeChanged()
{
    if (m_durationTypeComboBox->currentIndex() == kDurationOfEventIndex)
    {
        m_field->setValue(std::chrono::seconds::zero());
    }
    else
    {
        auto newValue = std::max(m_field->properties().value, m_field->properties().minimumValue);
        if (newValue == std::chrono::seconds::zero())
            newValue = std::chrono::seconds{1};

        m_field->setValue(newValue);
    }

    setEdited();
}

void ActionDurationPickerWidget::onDurationValueChanged()
{
    m_field->setValue(std::chrono::seconds{m_timeDurationWidget->value()});

    setEdited();
}

void ActionDurationPickerWidget::onStateChanged()
{
    const auto stateField = getEventField<vms::rules::StateField>(vms::rules::utils::kStateFieldName);
    stateField->setValue(m_actionStartComboBox->currentData().value<api::rules::State>());

    setEdited();
}

} // namespace nx::vms::client::desktop::rules
