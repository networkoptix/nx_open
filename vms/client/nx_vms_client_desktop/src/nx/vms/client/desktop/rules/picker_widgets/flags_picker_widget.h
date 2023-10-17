// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_level_field.h>

#include "picker_widget.h"
#include "picker_widget_strings.h"
#include "picker_widget_utils.h"
#include "plain_picker_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used for types represented by QFlags and displayed as a row of checkboxes.
 * Implementation depends on the Field parameter.
 * Has implementation for:
 * - nx.events.fields.analyticsEventLevel
 */
template<typename F>
class FlagsPicker: public PlainFieldPickerWidget<F>
{
    using base = PlainFieldPickerWidget<F>;

public:
    FlagsPicker(SystemContext* context, CommonParamsWidget* parent):
        base(context, parent)
    {
        auto flagsLayout = new QHBoxLayout;
        flagsLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
        for (const auto& [flag, displayString]: getFlags())
        {
            auto checkBox = new QCheckBox;
            checkBox->setText(displayString.toUpper());
            flagsLayout->addWidget(checkBox);

            m_checkBoxes.insert({checkBox, flag});

            connect(
                checkBox,
                &QCheckBox::toggled,
                this,
                &FlagsPicker::onValueChanged);
        }

        m_contentWidget->setLayout(flagsLayout);
    }

private:
    BASE_COMMON_USINGS

    std::map<QCheckBox*, typename F::value_type::enum_type> m_checkBoxes;

    void onValueChanged()
    {
        typename F::value_type newValue;
        for (const auto& [checkBox, flag]: m_checkBoxes)
        {
            if (checkBox->isChecked())
                newValue.setFlag(flag);
        }

        theField()->setValue(newValue);
    }

    void updateUi() override
    {
        const auto fieldValue = theField()->value();
        for (const auto& [checkBox, flag]: m_checkBoxes)
        {
            {
                QSignalBlocker blocker{checkBox};
                checkBox->setChecked(fieldValue.testFlag(flag));
            }
        }
    }

    // Returns map of the flag values and corresponding display string intended for displaying to
    // the user.
    std::map<typename F::value_type::enum_type, /*display string*/ QString> getFlags() const;
};

using AnalyticsEventLevelPicker = FlagsPicker<vms::rules::AnalyticsEventLevelField>;

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
