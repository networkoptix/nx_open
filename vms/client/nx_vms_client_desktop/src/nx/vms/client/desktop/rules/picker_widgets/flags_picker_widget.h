// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QCheckBox>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_level_field.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types represented by QFlags and displayed as a row of checkboxes.
 * Implementation depends on the Field parameter.
 * Has implementation for:
 * - nx.events.fields.analyticsEventLevel
 */
template<typename F>
class FlagsPickerWidget: public FieldPickerWidget<F>
{
public:
    FlagsPickerWidget(SystemContext* context, QWidget* parent = nullptr):
        FieldPickerWidget<F>(context, parent)
    {
        auto flagsLayout = new QHBoxLayout;
        flagsLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
        for (const auto& [flag, displayString]: getFlags())
        {
            auto checkBox = new QCheckBox;
            checkBox->setText(displayString.toUpper());
            flagsLayout->addWidget(checkBox);

            checkBoxes.insert({checkBox, flag});
        }

        m_contentWidget->setLayout(flagsLayout);
    }

private:
    PICKER_WIDGET_COMMON_USINGS

    std::map<QCheckBox*, typename F::value_type::enum_type> checkBoxes;

    virtual void onFieldsSet() override
    {
        const auto fieldValue = m_field->value();
        for (const auto& [checkBox, flag]: checkBoxes)
        {
            {
                QSignalBlocker blocker{checkBox};
                checkBox->setChecked(fieldValue.testFlag(flag));
            }

            connect(checkBox,
                &QCheckBox::toggled,
                this,
                &FlagsPickerWidget::onValueChanged,
                Qt::UniqueConnection);
        }
    }

    void onValueChanged()
    {
        typename F::value_type newValue;
        for (const auto& [checkBox, flag]: checkBoxes)
        {
            if (checkBox->isChecked())
                newValue.setFlag(flag);
        }

        m_field->setValue(newValue);
    }

    // Returns map of the flag values and corresponding display string intended for displaying to
    // the user.
    std::map<typename F::value_type::enum_type, /*display string*/ QString> getFlags() const;
};

using AnalyticsEventLevelPicker = FlagsPickerWidget<vms::rules::AnalyticsEventLevelField>;

template<>
std::map<api::EventLevel, QString> AnalyticsEventLevelPicker::getFlags() const
{
    return {
        {api::EventLevel::ErrorEventLevel,
            FlagsPickerWidgetStrings::eventLevelDisplayString(api::EventLevel::ErrorEventLevel)},
        {api::EventLevel::WarningEventLevel,
            FlagsPickerWidgetStrings::eventLevelDisplayString(api::EventLevel::WarningEventLevel)},
        {api::EventLevel::InfoEventLevel,
            FlagsPickerWidgetStrings::eventLevelDisplayString(api::EventLevel::InfoEventLevel)},
    };
}

} // namespace nx::vms::client::desktop::rules
