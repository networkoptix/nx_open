// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a time period. */
template<typename F>
class DurationPicker: public FieldPickerWidget<F>
{
public:
    DurationPicker(QnWorkbenchContext* context, CommonParamsWidget* parent):
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

        connect(
            m_timeDurationWidget,
            &TimeDurationWidget::valueChanged,
            this,
            &DurationPicker::onValueChanged);
    }

protected:
    PICKER_WIDGET_COMMON_USINGS
    TimeDurationWidget* m_timeDurationWidget;

    virtual void updateUi()
    {
        QSignalBlocker blocker{m_timeDurationWidget};
        m_timeDurationWidget->setValue(
            std::chrono::duration_cast<std::chrono::seconds>(theField()->value()).count());

        auto durationField =
            FieldPickerWidget<F>::template getActionField<vms::rules::OptionalTimeField>(
                vms::rules::utils::kDurationFieldName);

        if (!durationField)
            return;

        // TODO: #mmalofeev add property to the manifest to be able check dependencies.
        const bool isRecordingBeforeField =
            m_fieldDescriptor->fieldName == vms::rules::utils::kRecordBeforeFieldName;
        const bool isRecordingAfterField =
            m_fieldDescriptor->fieldName == vms::rules::utils::kRecordAfterFieldName;
        const bool belongsToBookmarkAction = parentParamsWidget()->descriptor().id
            == vms::rules::utils::type<vms::rules::BookmarkAction>();
        if ((isRecordingBeforeField && !belongsToBookmarkAction) || isRecordingAfterField)
        {
            if (durationField->value() == vms::rules::OptionalTimeField::value_type::zero())
                this->setEnabled(true);
            else
                this->setEnabled(false);
        }
    }

private:
    void onValueChanged()
    {
        theField()->setValue(std::chrono::seconds(m_timeDurationWidget->value()));
    }
};

template<typename F>
class OptionalDurationPicker: public DurationPicker<F>
{
public:
    OptionalDurationPicker(QnWorkbenchContext* context, CommonParamsWidget* parent = nullptr):
        DurationPicker<F>(context, parent)
    {
        m_label->setVisible(false);
        m_checkBox = new QCheckBox;
        static_cast<QHBoxLayout*>(m_contentWidget->layout())->insertWidget(0, m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &OptionalDurationPicker<F>::onStateChanged);
    }

private:
    PICKER_WIDGET_COMMON_USINGS
    using DurationPicker<F>::m_timeDurationWidget;

    QCheckBox* m_checkBox{nullptr};
    bool m_isDurationField{false};

    virtual void onDescriptorSet() override
    {
        DurationPicker<F>::onDescriptorSet();

        m_isDurationField = m_fieldDescriptor->fieldName == vms::rules::utils::kDurationFieldName;

        if (m_isDurationField)
            m_checkBox->setText(m_fieldDescriptor->displayName);
    }

    void updateUi() override
    {
        DurationPicker<F>::updateUi();

        const bool isZero = theField()->value() == F::value_type::zero();
        {
            const QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(!isZero);
        }

        if (m_isDurationField)
        {
            m_timeDurationWidget->setEnabled(m_checkBox->isChecked());
        }
        else
        {
            m_timeDurationWidget->setVisible(m_checkBox->isChecked());
            m_checkBox->setText(DurationPickerWidgetStrings::intervalOfActionHint(
                /*isInstant*/ !m_checkBox->isChecked()));
        }
    }

    void onStateChanged()
    {
        // TODO if event has state field change it
        if (m_checkBox->isChecked())
        {
            // Synced with the old rules dialog.
            constexpr auto kDefaultValue = std::chrono::seconds(60);

            theField()->setValue(kDefaultValue);
            if (m_isDurationField)
            {
                auto stateField = FieldPickerWidget<F>::template getEventField<vms::rules::StateField>(
                    vms::rules::utils::kStateFieldName);
                if (stateField && stateField->value() == vms::api::rules::State::none)
                {
                    const bool isInstantEvent =
                        parentParamsWidget()->eventDescriptor()->flags.testFlag(
                            vms::rules::ItemFlag::instant);
                    stateField->setValue(isInstantEvent
                        ? vms::api::rules::State::instant
                        : vms::api::rules::State::started);
                }
            }
        }
        else
        {
            theField()->setValue(F::value_type::zero());

            if (m_isDurationField)
            {
                auto stateField = FieldPickerWidget<F>::template getEventField<vms::rules::StateField>(
                    vms::rules::utils::kStateFieldName);
                if (stateField)
                    stateField->setValue(vms::api::rules::State::none);
            }
        }
    }
};

} // namespace nx::vms::client::desktop::rules
