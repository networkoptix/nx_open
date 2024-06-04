// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_builder_fields/target_server_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>

#include "../utils/icons.h"
#include "../utils/strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename Policy>
class ServerPickerWidgetBase: public ResourcePickerWidgetBase<F>
{
    using base = ResourcePickerWidgetBase<F>;

public:
    using ResourcePickerWidgetBase<F>::ResourcePickerWidgetBase;

protected:
    BASE_COMMON_USINGS
    using ResourcePickerWidgetBase<F>::m_selectButton;
    using ResourcePickerWidgetBase<F>::updateUi;
    using ResourcePickerWidgetBase<F>::systemContext;

    void updateUi() override
    {
        ResourcePickerWidgetBase<F>::updateUi();

        m_selectButton->setText(Strings::selectButtonText(systemContext(), m_field));
        m_selectButton->setIcon(selectButtonIcon(systemContext(), m_field));
    }

    void onSelectButtonClicked() override
    {
        auto selectedServers = m_field->ids();

        if (ServerSelectionDialog::selectServers(
            selectedServers, Policy::isServerValid, Policy::infoText(), this))
        {
            m_field->setIds(selectedServers);
            updateUi();
        }
    }
};

template<typename Policy>
class SourceServerPicker: public ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>
{
public:
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::ServerPickerWidgetBase;
};

template<typename Policy>
class TargetServerPicker: public ServerPickerWidgetBase<vms::rules::TargetServerField, Policy>
{
public:
    using ServerPickerWidgetBase<vms::rules::TargetServerField, Policy>::ServerPickerWidgetBase;
};

} // namespace nx::vms::client::desktop::rules
