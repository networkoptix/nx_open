// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Utils 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0
import Nx.InteractiveSettings 1.0

Item
{
    id: analyticsSettings

    property var store: null
    property var engineLicenseSummaryProvider: null
    property var saasServiceManager: null

    function activateEngine(engineId) { viewModel.setCurrentEngine(engineId) }

    AnalyticsSettingsViewModel
    {
        id: viewModel

        requestsModel: store ? store.makeApiIntegrationRequestsModel() : null

        onEngineRequested: (engineId) =>
        {
            if (store)
                store.setCurrentEngineId(engineId ?? NxGlobals.uuid(""))
        }

        function storeUpdated()
        {
            const engineId = store.getCurrentEngineId()
            const licenseSummary = engineLicenseSummaryProvider.licenseSummary(engineId)
            viewModel.updateState(
                store.analyticsEngines,
                licenseSummary,
                engineId,
                store.settingsModel(engineId),
                store.settingsValues(engineId),
                store.errors(engineId))
        }

        Connections
        {
            target: analyticsSettings.store

            function onAnalyticsEnginesChanged() { viewModel.storeUpdated() }
            function onCurrentSettingsStateChanged() { viewModel.storeUpdated() }
            function onCurrentErrorsChanged() { viewModel.storeUpdated() }
            function onLicenseSummariesChanged() { viewModel.storeUpdated() }
        }
    }

    AnalyticsSettingsMenu
    {
        id: menu

        width: 240
        height: parent.height - saasBanner.height

        viewModel: viewModel
    }

    Item
    {
        id: scrollBarParent

        width: 8

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    AnalyticsSettingsView
    {
        id: settings

        viewModel: viewModel
        enabled: !saasBanner.visible

        anchors.top: parent.top
        anchors.bottom: saasBanner.top
        anchors.left: menu.right
        anchors.right: scrollBarParent.left
        anchors.margins: 16

        settingsView
        {
            enabled: !!store && !store.loading
            scrollBarParent: scrollBarParent

            requestParametersFunction: (model) => { return store.requestParameters(model) }

            onValuesEdited: (activeItem, parameters) =>
            {
                store.setSettingsValues(
                    viewModel.currentEngineId,
                    activeItem ? activeItem.name : "",
                    settings.settingsView.getValues(),
                    parameters)
            }
        }

        settingsViewHeader
        {
            checkable: false
            refreshable: false
            streamSelectorVisible: false
        }

        settingsViewPlaceholder
        {
            header: qsTr("This plugin has no settings at the System level.")
            description: qsTr("Check Camera Settings to configure this plugin.")
            loading: !!store && store.loading
        }

        Binding
        {
            target: viewModel.requestsModel
            property: "isActive"
            value: settings.visible
        }
    }

    SaasBanner
    {
        id: saasBanner

        saasServiceManager: analyticsSettings.saasServiceManager
        licenseSummary: viewModel.currentEngineLicenseSummary
        deviceSpecific: false

        height: visible ? implicitHeight : 0
        anchors.bottom: parent.bottom
        width: parent.width
    }

    onStoreChanged: store.setCurrentEngineId(viewModel.currentEngineId ?? NxGlobals.uuid(""))
}
