// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Controls.NavigationMenu
import Nx.Items

import nx.vms.client.desktop

import "private/AnalyticsSettingsMenu"

Control
{
    id: control
    required property var viewModel

    topPadding: 8
    bottomPadding: 8

    component Separator: Column
    {
        topPadding: 8
        bottomPadding: 8

        width: parent ? parent.width : 100

        Rectangle
        {
            width: parent.width
            height: 1
            color: ColorTheme.colors.dark6
        }

        Rectangle
        {
            width: parent.width
            height: 1
            color: ColorTheme.colors.dark9
        }

    }

    contentItem: ColumnLayout
    {
        spacing: 0

        SearchField
        {
            id: search
            Layout.margins: 8
            Layout.fillWidth: true
        }

        NavigationMenu
        {
            id: menu

            Layout.fillHeight: true
            Layout.fillWidth: true

            scrollView.topPadding: 0
            scrollView.bottomPadding: 0

            EngineList
            {
                width: parent.width
                level: 0
                navigationMenu: menu
                viewModel: control.viewModel
                engines: filteredRequests
            }

            Separator { visible: filteredRequests.count > 0 && filteredEngines.count > 0 }

            EngineList
            {
                width: parent.width
                level: 0
                navigationMenu: menu
                viewModel: control.viewModel
                engines: filteredEngines
                enabledEngines: viewModel.enabledEngines
            }

            function setCurrentItem(engineId, sectionId, requestId)
            {
                viewModel.setCurrentItem("Engine", engineId, sectionId, requestId)
            }

            function getId(engineId, sectionId, requestId)
            {
                return sectionId || engineId || requestId
            }

            Connections
            {
                target: viewModel

                function onCurrentItemChanged()
                {
                    menu.currentItemId = menu.getId(
                        viewModel.currentEngineId,
                        viewModel.currentSectionId,
                        viewModel.currentRequestId)
                }
            }
        }
    }

    SortFilterProxyModel
    {
        id: filteredRequests
        sourceModel: viewModel.requestListModel
        sortRoleName: "name"
        filterRoleName: "name"
        filterRegularExpression: new RegExp(search.text, "i")
    }

    SortFilterProxyModel
    {
        id: filteredEngines
        sourceModel: viewModel.engineListModel
        sortRoleName: "name"
        filterRoleName: "name"
        filterRegularExpression: new RegExp(search.text, "i")
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark8
    }
}
