// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/icon_combo_box.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <nx/vms/rules/event_filter_fields/customizable_icon_field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class CustomizableIconPicker:
    public DropdownTextPickerWidgetBase<vms::rules::CustomizableIconField, IconComboBox>
{
public:
    CustomizableIconPicker(
        vms::rules::CustomizableIconField* field,
        SystemContext* context,
        ParamsWidget* parent)
        :
        DropdownTextPickerWidgetBase<vms::rules::CustomizableIconField, IconComboBox>(
            field, context, parent)
    {
        constexpr auto kDropdownIconSize = 40;
        const auto pixmaps = SoftwareTriggerPixmaps::pixmaps();
        const auto nextEvenValue = [](int value) { return value + (value & 1); };
        const auto columnCount = nextEvenValue(qCeil(qSqrt(pixmaps.size())));

        QSignalBlocker blocker{m_comboBox};
        m_comboBox->setColumnCount(columnCount);
        m_comboBox->setItemSize({kDropdownIconSize, kDropdownIconSize});
        m_comboBox->setPixmaps(pixmaps);
        m_comboBox->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));

        auto contentLayout = qobject_cast<QHBoxLayout*>(m_contentWidget->layout());
        if (NX_ASSERT(contentLayout))
            contentLayout->addStretch();
    }

protected:
    void onActivated() override
    {
        m_field->setValue(m_comboBox->currentIcon());

        DropdownTextPickerWidgetBase<vms::rules::CustomizableIconField, IconComboBox>::onActivated();
    }

    void updateUi() override
    {
        DropdownTextPickerWidgetBase<vms::rules::CustomizableIconField, IconComboBox>::updateUi();

        m_comboBox->setCurrentIcon(SoftwareTriggerPixmaps::effectivePixmapName(m_field->value()));
    }
};

} // namespace nx::vms::client::desktop::rules
