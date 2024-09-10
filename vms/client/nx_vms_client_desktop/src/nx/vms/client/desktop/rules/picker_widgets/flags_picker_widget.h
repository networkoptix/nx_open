// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/rules/event_filter_fields/analytics_event_level_field.h>

#include "../utils/strings.h"
#include "field_picker_widget.h"
#include "picker_widget_utils.h"

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
    FlagsPicker(F* field, SystemContext* context, ParamsWidget* parent):
        base(field, context, parent)
    {
        auto flagsLayout = new QHBoxLayout{m_contentWidget};
        flagsLayout->setSpacing(style::Metrics::kDefaultLayoutSpacing.width());
        for (const auto& [flag, displayString]: getFlags())
        {
            auto checkBox = new QCheckBox;
            checkBox->setText(displayString);
            flagsLayout->addWidget(checkBox);

            m_checkBoxes.insert({checkBox, flag});

            connect(
                checkBox,
                &QCheckBox::toggled,
                this,
                &FlagsPicker::onValueChanged);
        }

        flagsLayout->addSpacerItem(
            new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum));
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

        m_field->setValue(newValue);

        setEdited();
    }

    void updateUi() override
    {
        PlainFieldPickerWidget<F>::updateUi();

        const auto fieldValue = m_field->value();
        for (const auto& [checkBox, flag]: m_checkBoxes)
        {
            QSignalBlocker blocker{checkBox};
            checkBox->setChecked(fieldValue.testFlag(flag));
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
        {api::EventLevel::error, Strings::eventLevelDisplayString(api::EventLevel::error)},
        {api::EventLevel::warning, Strings::eventLevelDisplayString(api::EventLevel::warning)},
        {api::EventLevel::info, Strings::eventLevelDisplayString(api::EventLevel::info)},
    };
}

} // namespace nx::vms::client::desktop::rules
