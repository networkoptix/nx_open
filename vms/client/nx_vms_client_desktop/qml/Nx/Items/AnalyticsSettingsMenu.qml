// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls.NavigationMenu 1.0

import nx.vms.client.desktop 1.0

import "private/AnalyticsSettingsMenu"

NavigationMenu
{
    id: menu

    required property var viewModel
    readonly property int sectionWidth: width - scrollBarWidth

    implicitWidth: 240

    NxObject
    {
        id: impl

        readonly property var sectionIds: ({
            "Plugins": plugins,
            "ApiIntegrations": apiIntegrations
        })

        Connections
        {
            target: viewModel

            function onCurrentItemChanged()
            {
                menu.currentItemId =
                    viewModel.currentSectionId
                    || viewModel.currentEngineId
                    || impl.sectionIds[viewModel.currentItemType]
            }
        }
    }

    function setCurrentItem(engineId, sectionId, requestId)
    {
        viewModel.setCurrentItem("Engine", engineId, sectionId, requestId)
    }

    function getId(engineId, sectionId, requestId)
    {
        return sectionId || requestId || engineId
    }

    MenuSection
    {
        id: plugins

        property var engines: LocalSettings.iniConfigValue("enableMetadataApi")
            ? viewModel.engines.filter(engine => engine.type === "sdk")
            : viewModel.engines

        mainItemVisible: LocalSettings.iniConfigValue("enableMetadataApi")

        text: qsTr("Plugins")
        font.weight: Font.Medium
        width: sectionWidth

        content: EngineList
        {
            navigationMenu: menu
            viewModel: menu.viewModel
            engines: plugins.engines
            enabledEngines: viewModel.enabledEngines
            level: plugins.mainItemVisible ? 1 : 0
        }

        onClicked:
            viewModel.setCurrentItem("Plugins", /*engineId*/ null, /*sectionId*/ null)

        onCollapsedChanged:
        {
            if (!collapsed)
                apiIntegrations.collapsed = true
        }
    }

    MenuSection
    {
        id: apiIntegrations

        property var engines: viewModel.engines.filter(engine => engine.type === "api")
        property var requestCount: viewModel.requests ? viewModel.requests.length : 0

        text: qsTr("API Integrations")
        indicatorText: requestCount > 0 ? `+${requestCount}` : ""
        collapsible: engines.length > 0
        collapsed: true
        visible: LocalSettings.iniConfigValue("enableMetadataApi")
        font.weight: Font.Medium
        width: sectionWidth

        content: EngineList
        {
            navigationMenu: menu
            viewModel: menu.viewModel
            engines: apiIntegrations.engines
            enabledEngines: viewModel.enabledEngines
        }

        onClicked:
            viewModel.setCurrentItem("ApiIntegrations", /*engineId*/ null, /*sectionId*/ null)

        onCollapsedChanged:
        {
            if (!collapsed)
                plugins.collapsed = true
        }
    }
}
