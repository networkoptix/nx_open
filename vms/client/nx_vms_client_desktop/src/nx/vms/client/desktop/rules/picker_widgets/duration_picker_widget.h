// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a time period. */
template<typename F>
class DurationPickerWidget: public FieldPickerWidget<F>
{
public:
    DurationPickerWidget(SystemContext* context, CommonParamsWidget* parent):
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
            &DurationPickerWidget::onValueChanged);
    }

protected:
    TimeDurationWidget* m_timeDurationWidget;
    nx::utils::ScopedConnections m_scopedConnections;

    virtual void onActionBuilderChanged() override
    {
        static_assert(std::is_base_of<vms::rules::ActionBuilderField, F>());

        FieldPickerWidget<F>::onActionBuilderChanged();

        {
            QSignalBlocker blocker{m_timeDurationWidget};
            m_timeDurationWidget->setValue(
                std::chrono::duration_cast<std::chrono::seconds>(m_field->value()).count());
        }

        m_scopedConnections.reset();

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
            const auto updateEnabledState =
                [this, durationField]
                {
                    if (durationField->value() == vms::rules::OptionalTimeField::value_type::zero())
                        this->setEnabled(true);
                    else
                        this->setEnabled(false);
                };

            updateEnabledState();

            m_scopedConnections << connect(
                durationField,
                &vms::rules::OptionalTimeField::valueChanged,
                this,
                updateEnabledState);
        }
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    void onValueChanged()
    {
        m_field->setValue(std::chrono::seconds(m_timeDurationWidget->value()));
    }
};

template<typename F>
class OptionalDurationPickerWidget: public DurationPickerWidget<F>
{
public:
    OptionalDurationPickerWidget(SystemContext* context, CommonParamsWidget* parent = nullptr):
        DurationPickerWidget<F>(context, parent)
    {
        m_label->setVisible(false);
        m_checkBox = new QCheckBox;
        static_cast<QHBoxLayout*>(m_contentWidget->layout())->insertWidget(0, m_checkBox);

        connect(
            m_checkBox,
            &QCheckBox::stateChanged,
            this,
            &OptionalDurationPickerWidget<F>::onStateChanged);
    }

private:
    PICKER_WIDGET_COMMON_USINGS
    using DurationPickerWidget<F>::m_timeDurationWidget;

    QCheckBox* m_checkBox{nullptr};
    bool m_isDurationField{false};
    typename F::value_type m_initialValue;

    virtual void onDescriptorSet() override
    {
        DurationPickerWidget<F>::onDescriptorSet();

        m_isDurationField = m_fieldDescriptor->fieldName == vms::rules::utils::kDurationFieldName;

        if (m_isDurationField)
            m_checkBox->setText(m_fieldDescriptor->displayName);
    }

    virtual void onActionBuilderChanged() override
    {
        DurationPickerWidget<F>::onActionBuilderChanged();

        m_initialValue = m_field->value();
        const bool isZero = m_initialValue == F::value_type::zero();

        {
            const QSignalBlocker blocker{m_checkBox};
            m_checkBox->setChecked(!isZero);
        }

        if (m_isDurationField)
        {
            m_timeDurationWidget->setEnabled(!isZero);
        }
        else
        {
            m_timeDurationWidget->setVisible(!isZero);
            updateCheckBoxText();
        }
    }

    void onStateChanged()
    {
        if (m_checkBox->isChecked())
        {
            const bool useInitialValue = m_initialValue != F::value_type::zero();
            // Synced with the old rules dialog.
            constexpr auto kDefaultValue = std::chrono::seconds(60);

            m_field->setValue(useInitialValue ? m_initialValue : kDefaultValue);

            QSignalBlocker blocker{m_timeDurationWidget};
            m_timeDurationWidget->setValue(useInitialValue
                ? std::chrono::duration_cast<std::chrono::seconds>(m_initialValue).count()
                : kDefaultValue.count());
        }
        else
        {
            m_field->setValue(F::value_type::zero());
        }

        if (m_isDurationField)
        {
            m_timeDurationWidget->setEnabled(m_checkBox->isChecked());
        }
        else
        {
            m_timeDurationWidget->setVisible(m_checkBox->isChecked());
            updateCheckBoxText();
        }
    }

    void updateCheckBoxText()
    {
        m_checkBox->setText(DurationPickerWidgetStrings::intervalOfActionHint(
            /*isInstant*/ !m_checkBox->isChecked()));
    }
};

} // namespace nx::vms::client::desktop::rules
