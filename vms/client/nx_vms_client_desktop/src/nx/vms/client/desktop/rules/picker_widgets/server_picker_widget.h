// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/event_filter_fields/source_server_field.h>

#include "picker_widget_strings.h"
#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename Policy>
class ServerPickerWidgetBase: public ResourcePickerWidgetBase<F>
{
public:
    using ResourcePickerWidgetBase<F>::ResourcePickerWidgetBase;

protected:
    PICKER_WIDGET_COMMON_USINGS
    using ResourcePickerWidgetBase<F>::m_selectButton;
    using ResourcePickerWidgetBase<F>::updateUi;

    void onSelectButtonClicked() override
    {
        auto field = theField();
        auto selectedServers = field->ids();

        if (ServerSelectionDialog::selectServers(
            selectedServers, Policy::isServerValid, Policy::infoText(), this))
        {
            field->setIds(selectedServers);
            updateUi();
        }
    }
};

template<typename Policy>
class SourceServerPicker: public ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>
{
public:
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::ServerPickerWidgetBase;

protected:
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::theField;
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::m_selectButton;
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::resourcePool;

    void updateUi() override
    {
        const auto resources =
            resourcePool()->template getResourcesByIds<QnMediaServerResource>(theField()->ids());

        QIcon icon;
        if (resources.isEmpty())
        {
            icon = qnResIconCache->icon(QnResourceIconCache::Server);
            m_selectButton->setText(ServerPickerStrings::anyServerString());
        }
        else
        {
            icon = (resources.size() == 1)
                ? qnResIconCache->icon(QnResourceIconCache::Server)
                : qnResIconCache->icon(QnResourceIconCache::Servers);
            m_selectButton->setText(resources.size() == 1
                ? resources.first()->getName()
                : ServerPickerStrings::multipleServersString(resources.size()));
        }

        m_selectButton->setIcon(Skin::maximumSizePixmap(
            icon,
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false));
    }
};

template<typename Policy>
class TargetServerPicker: public ServerPickerWidgetBase<vms::rules::TargetDeviceField, Policy>
{
public:
    using ServerPickerWidgetBase<vms::rules::TargetDeviceField, Policy>::ServerPickerWidgetBase;

protected:
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::theField;
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::m_selectButton;
    using ServerPickerWidgetBase<vms::rules::SourceServerField, Policy>::resourcePool;

    void updateUi() override
    {
        auto field = theField();
        const auto resources =
            resourcePool()->template getResourcesByIds<QnMediaServerResource>(field->ids());

        QIcon icon;
        if (resources.isEmpty())
        {
            icon = icon = qnResIconCache->icon(QnResourceIconCache::Server);
            m_selectButton->setText(field->useSource()
                ? ServerPickerStrings::sourceServerString(resources.size())
                : ServerPickerStrings::selectServerString());
        }
        else
        {
            m_selectButton->setText(field->useSource()
                ? ServerPickerStrings::sourceServerString(resources.size())
                : ServerPickerStrings::multipleServersString(resources.size()));

            icon = (resources.size() > 1 || field->useSource())
                ? qnResIconCache->icon(QnResourceIconCache::Servers)
                : qnResIconCache->icon(QnResourceIconCache::Server);
        }

        m_selectButton->setIcon(Skin::maximumSizePixmap(
            icon,
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false));
    }
};

} // namespace nx::vms::client::desktop::rules
