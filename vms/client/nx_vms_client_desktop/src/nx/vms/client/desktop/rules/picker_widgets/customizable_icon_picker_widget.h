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
    CustomizableIconPicker(SystemContext* context, CommonParamsWidget* parent):
        DropdownTextPickerWidgetBase<vms::rules::CustomizableIconField, IconComboBox>(context, parent)
    {
        constexpr auto kDropdownIconSize = 40;
        const auto pixmapNames = SoftwareTriggerPixmaps::pixmapNames();
        const auto nextEvenValue = [](int value) { return value + (value & 1); };
        const auto columnCount = nextEvenValue(qCeil(qSqrt(pixmapNames.size())));

        QSignalBlocker blocker{m_comboBox};
        m_comboBox->setColumnCount(columnCount);
        m_comboBox->setItemSize({kDropdownIconSize, kDropdownIconSize});
        m_comboBox->setPixmaps(SoftwareTriggerPixmaps::pixmapsPath(), pixmapNames);
    }

protected:
    void onCurrentIndexChanged() override
    {
        theField()->setValue(m_comboBox->currentIcon());
    }

    void updateUi() override
    {
        QSignalBlocker blocker{m_comboBox};
        m_comboBox->setCurrentIcon(SoftwareTriggerPixmaps::effectivePixmapName(theField()->value()));
    }
};

} // namespace nx::vms::client::desktop::rules
