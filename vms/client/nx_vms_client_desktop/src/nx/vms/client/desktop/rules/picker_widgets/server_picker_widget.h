// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>
#include <ui/widgets/select_resources_button.h>

#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F>
class ServerPickerWidget: public ResourcePickerWidgetBase<F, QnSelectServersButton>
{
public:
    using ResourcePickerWidgetBase<F, QnSelectServersButton>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F, QnSelectServersButton>::m_selectButton;
    using ResourcePickerWidgetBase<F, QnSelectServersButton>::m_alertLabel;

    void onSelectButtonClicked() override
    {
        auto selectedServers = m_field->ids();

        const auto serverFilter = [](const QnMediaServerResourcePtr&){ return true; };
        ServerSelectionDialog::selectServers(selectedServers, serverFilter, QString{}, this);

        m_field->setIds(selectedServers);

        updateValue();
    }

    void updateValue() override
    {
        auto resourceList =
            resourcePool()->template getResourcesByIds<QnMediaServerResource>(m_field->ids());

        m_selectButton->selectServers(resourceList);

        if (resourceList.isEmpty())
        {
            m_alertLabel->setText(ResourcePickerWidgetStrings::selectServer());
            m_alertLabel->setVisible(true);
        }
        else
        {
            m_alertLabel->setVisible(false);
        }
    }
};

using ServerPicker = ServerPickerWidget<vms::rules::SourceServerField>;

} // namespace nx::vms::client::desktop::rules
