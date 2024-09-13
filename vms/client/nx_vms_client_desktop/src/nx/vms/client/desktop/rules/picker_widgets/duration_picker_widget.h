// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpinBox>

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/time_field.h>
#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/common/read_only.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "common.h"
#include "field_picker_widget.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a time period. */
template<typename F>
class DurationPicker: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    DurationPicker(F* field, SystemContext* context, ParamsWidget* parent):
        base(field, context, parent)
    {
        auto contentLayout = new QHBoxLayout{m_contentWidget};

        m_timeDurationWidget = new TimeDurationWidget;
        m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Minutes);
        m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Hours);
        m_timeDurationWidget->addDurationSuffix(QnTimeStrings::Suffix::Days);

        const auto fieldProperties = field->properties();
        m_timeDurationWidget->setMaximum(fieldProperties.maximumValue.count());
        m_timeDurationWidget->setMinimum(fieldProperties.minimumValue.count());

        m_timeDurationWidget->valueSpinBox()->setFixedWidth(kDurationValueSpinBoxWidth);

        contentLayout->addWidget(m_timeDurationWidget);

        connect(
            m_timeDurationWidget,
            &TimeDurationWidget::valueChanged,
            this,
            &DurationPicker::onValueChanged);
    }

protected:
    BASE_COMMON_USINGS

    TimeDurationWidget* m_timeDurationWidget{nullptr};

    virtual void updateUi() override
    {
        PlainFieldPickerWidget<F>::updateUi();

        const auto durationField =
            base::template getActionField<vms::rules::OptionalTimeField>(
                vms::rules::utils::kDurationFieldName);

        if (durationField)
        {
            const bool isFixedDuration = durationField
                && durationField->value() != vms::rules::OptionalTimeField::value_type::zero();

            if (m_field->descriptor()->fieldName == vms::rules::utils::kRecordBeforeFieldName)
            {
                base::setDisplayName(isFixedDuration
                    ? TimeDurationWidget::tr(
                        "Also include",
                        /*comment*/ "Part of the text, action duration: "
                        "Also include <time> Before Event")
                    : TimeDurationWidget::tr(
                        "Begin",
                        /*comment*/ "Part of the text, action duration: "
                        "Begin <time> Before Event"));

                m_timeDurationWidget->setAdditionalText(TimeDurationWidget::tr(
                    "Before Event",
                    /*comment*/ "Part of the text, action duration: "
                    "Begin <time> Before Event"));
            }

            if (m_field->descriptor()->fieldName == vms::rules::utils::kRecordAfterFieldName)
            {
                base::setDisplayName(TimeDurationWidget::tr(
                    "End",
                    /*comment*/ "Part of the text, action duration: "
                    "End <time> After Event"));

                m_timeDurationWidget->setAdditionalText(TimeDurationWidget::tr(
                    "After Event",
                    /*comment*/ "Part of the text, action duration: "
                    "End <time> After Event"));

                // Post recording must not be visible when fixed duration is set.
                this->setVisible(!isFixedDuration);
            }
        }

        QSignalBlocker blocker{m_timeDurationWidget};
        m_timeDurationWidget->setValue(
            std::chrono::duration_cast<std::chrono::seconds>(m_field->value()).count());
    }

private:
    void onValueChanged()
    {
        m_field->setValue(std::chrono::seconds{m_timeDurationWidget->value()});

        setEdited();
    }
};

} // namespace nx::vms::client::desktop::rules
