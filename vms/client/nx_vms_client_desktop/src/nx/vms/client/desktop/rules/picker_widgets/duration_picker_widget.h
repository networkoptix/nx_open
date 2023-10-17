// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/action_builder_fields/time_field.h>
#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/common/read_only.h>
#include <ui/widgets/business/time_duration_widget.h>

#include "field_picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/** Used for types that could be represented as a time period. */
template<typename F>
class DurationPicker: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    DurationPicker(SystemContext* context, CommonParamsWidget* parent):
        base(context, parent)
    {
        auto contentLayout = new QHBoxLayout;

        m_timeDurationWidget = new TimeDurationWidget;
        m_timeDurationWidget->setSizePolicy(
            QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred));

        contentLayout->addWidget(m_timeDurationWidget);

        m_contentWidget->setLayout(contentLayout);

        connect(
            m_timeDurationWidget,
            &TimeDurationWidget::valueChanged,
            this,
            &DurationPicker::onValueChanged);
    }

protected:
    BASE_COMMON_USINGS
    TimeDurationWidget* m_timeDurationWidget{nullptr};

    virtual void onDescriptorSet() override
    {
        base::onDescriptorSet();

        auto maxIt = m_fieldDescriptor->properties.constFind("max");
        if (maxIt != m_fieldDescriptor->properties.constEnd())
            m_timeDurationWidget->setMaximum(maxIt->template value<std::chrono::seconds>().count());

        auto minIt = m_fieldDescriptor->properties.constFind("min");
        m_timeDurationWidget->setMinimum(minIt == m_fieldDescriptor->properties.constEnd()
            ? 0
            : minIt->template value<std::chrono::seconds>().count());
    }

    virtual void updateUi() override
    {
        auto field = theField();

        if (m_fieldDescriptor->linkedFields.contains(vms::rules::utils::kDurationFieldName))
        {
            const auto durationField =
                base::template getActionField<vms::rules::OptionalTimeField>(
                    vms::rules::utils::kDurationFieldName);
            if (!NX_ASSERT(durationField))
                return;

            const auto hasDuration =
                durationField->value() != vms::rules::OptionalTimeField::value_type::zero();
            if (hasDuration && field->value() != F::value_type::zero())
            {
                field->setValue(F::value_type::zero());
                return;
            }

            this->setVisible(!hasDuration);
        }

        QSignalBlocker blocker{m_timeDurationWidget};
        m_timeDurationWidget->setValue(
            std::chrono::duration_cast<std::chrono::seconds>(field->value()).count());
    }

private:
    void onValueChanged()
    {
        theField()->setValue(std::chrono::seconds{m_timeDurationWidget->value()});
    }
};

} // namespace nx::vms::client::desktop::rules
