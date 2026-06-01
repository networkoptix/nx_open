// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <nx/vms/rules/action_builder_fields/target_server_field.h>
#include <nx/vms/rules/action_builder_fields/target_servers_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>

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

    void onSelectButtonClicked() override
    {
        auto selectedServers = m_field->ids();

        if (ServerSelectionDialog::selectServers(
            selectedServers, Policy::isServerValid, Policy::infoText(), this))
        {
            m_field->setIds(selectedServers);
        }

        ResourcePickerWidgetBase<F>::onSelectButtonClicked();
    }
};

template<typename Policy>
class SourceServerPicker: public ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>
{
public:
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::ServerPickerWidgetBase;
};

template<typename Policy>
class TargetServersPicker: public ServerPickerWidgetBase<vms::rules::TargetServersField, Policy>
{
public:
    using ServerPickerWidgetBase<vms::rules::TargetServersField, Policy>::ServerPickerWidgetBase;
};

template<typename Policy>
class TargetServerPicker: public ResourcePickerWidgetBase<vms::rules::TargetServerField>
{
    using base = ResourcePickerWidgetBase<vms::rules::TargetServerField>;

public:
    using base::base;

protected:
    BASE_COMMON_USINGS

    void onSelectButtonClicked() override
    {
        auto callback = nx::utils::guarded(this,
            [this](bool success, nx::Uuid selectedServer)
            {
                if (success)
                    m_field->setValue(selectedServer);
            });

        ServerSelectionDialog::selectServer(
            m_field->value(), Policy::isServerValid, Policy::infoText(), std::move(callback), this);

        base::onSelectButtonClicked();
    }
};

} // namespace nx::vms::client::desktop::rules
