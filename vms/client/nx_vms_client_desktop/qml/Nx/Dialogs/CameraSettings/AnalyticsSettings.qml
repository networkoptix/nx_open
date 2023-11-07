// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0
import Nx.Core 1.0
import Nx.InteractiveSettings 1.0
import Nx.Items 1.0
import Nx.Utils 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Item
{
    id: analyticsSettings

    property var store: null
    property var backend: null
    property var engineLicenseSummaryProvider: null
    property var saasServiceManager: null

    property bool loading: false
    property bool supportsDualStreaming: false
    readonly property bool isDeviceDependent: !!viewModel.currentEngineInfo
        && viewModel.currentEngineInfo.isDeviceDependent

    readonly property var currentEngineId: viewModel.currentEngineId
    property var resourceId: NxGlobals.uuid("")
    property bool initialized: false
    property alias viewModel: viewModel

    Connections
    {
        target: store

        function onStateModified()
        {
            const resourceId = store.resourceId()
            if (resourceId.isNull())
                return

            loading = store.analyticsSettingsLoading()
            if (loading)
                return

            supportsDualStreaming = store.dualStreamingEnabled()
            analyticsSettings.resourceId = resourceId
            const isInitial =
                !analyticsSettings.initialized || analyticsSettings.resourceId !== resourceId

            const currentEngineId = store.currentAnalyticsEngineId()
            const userEnabledAnalyticsEngines = store.userEnabledAnalyticsEngines()
            const licenseSummary = engineLicenseSummaryProvider.licenseSummary(
                currentEngineId, resourceId, userEnabledAnalyticsEngines)
            viewModel.enabledEngines = userEnabledAnalyticsEngines
            viewModel.updateState(
                store.analyticsEngines(),
                licenseSummary,
                store.deviceAgentSettingsModel(currentEngineId),
                store.deviceAgentSettingsValues(currentEngineId),
                store.deviceAgentSettingsErrors(currentEngineId),
                isInitial)

            analyticsSettings.initialized = true

            if (currentEngineId)
            {
                analyticsSettingsView.settingsViewHeader.currentStreamIndex =
                    store.analyticsStreamIndex(currentEngineId)
            }

            recordingNotEnabledBanner.visible =
                !store.recordingEnabled() && viewModel.enabledEngines.length !== 0
        }
    }

    MediaResourceHelper
    {
        id: mediaResourceHelper

        resourceId: analyticsSettings.resourceId
    }

    AnalyticsSettingsViewModel
    {
        id: viewModel

        onEngineRequested: (engineId) =>
        {
            if (store)
                store.setCurrentAnalyticsEngineId(engineId ?? NxGlobals.uuid(""))
        }
    }

    Component
    {
        id: analyticsSettingsMenu
        AnalyticsSettingsMenu { viewModel: analyticsSettings.viewModel }
    }

    Component
    {
        id: integrationsMenu
        IntegrationsMenu { viewModel: analyticsSettings.viewModel }
    }

    Loader
    {
        id: navigationMenu
        width: 240
        height: parent.height - banners.height

        sourceComponent: LocalSettings.iniConfigValue("integrationsManagement")
            ? integrationsMenu
            : analyticsSettingsMenu
    }

    AnalyticsSettingsView
    {
        id: analyticsSettingsView

        viewModel: viewModel
        enabled: !saasBanner.visible

        x: navigationMenu.width + 16
        y: 16
        width: parent.width - x - 24
        height: parent.height - 16 - banners.height

        settingsView
        {
            enabled: !loading
            contentEnabled: settingsViewHeader.checked || isDeviceDependent
            contentVisible: settingsView.contentEnabled
            scrollBarParent: scrollBarsParent

            thumbnailSource: RoiCameraThumbnail
            {
                cameraId: analyticsSettings.resourceId
                active: visible
            }

            requestParametersFunction: (model) => { return backend.requestParameters(model) }

            onValuesEdited: (activeItem, parameters) =>
            {
                store.setDeviceAgentSettingsValues(
                    currentEngineId,
                    activeItem ? activeItem.name : "",
                    analyticsSettingsView.settingsView.getValues(),
                    parameters)
            }
        }

        settingsViewHeader
        {
            checkable: !isDeviceDependent && !!currentEngineId
            refreshable: settingsViewHeader.checked || !settingsViewHeader.checkable
            refreshing: analyticsSettings.loading
            streamSelectorVisible:
                supportsDualStreaming && settingsViewHeader.currentStreamIndex >= 0
            streamSelectorEnabled: settingsView.contentEnabled

            onEnableSwitchClicked:
            {
                if (currentEngineId)
                    setEngineEnabled(currentEngineId, !settingsViewHeader.checked)
            }

            onRefreshButtonClicked:
            {
                if (currentEngineId)
                {
                    analyticsSettings.initialized = false
                    store.refreshDeviceAgentSettings(currentEngineId)
                }
            }

            onCurrentStreamIndexChanged:
            {
                if (currentEngineId)
                {
                    store.setAnalyticsStreamIndex(
                        currentEngineId,
                        settingsViewHeader.currentStreamIndex)
                }
            }
        }

        settingsViewPlaceholder
        {
            header: qsTr("This plugin has no settings for this camera.")
            description: qsTr("Check System Administration settings to configure this plugin.")
            loading: analyticsSettings.loading
        }
    }

    Item
    {
        id: scrollBarsParent
        width: parent.width
        height: banners.y
    }

    Column
    {
        id: banners
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 8

        SaasBanner
        {
            id: saasBanner

            width: parent.width
            saasServiceManager: analyticsSettings.saasServiceManager
            licenseSummary: viewModel.currentEngineLicenseSummary
            deviceSpecific: true
        }

        DialogBanner
        {
            id: recordingNotEnabledBanner

            visible: false
            width: parent.width
            style: DialogBanner.Style.Warning
            text: qsTr("Camera analytics will work only when camera is being viewed."
                + " Enable recording to make it work all the time.")
        }
    }

    function setEngineEnabled(engineId, enabled)
    {
        let engines = viewModel.enabledEngines.slice(0)
        if (enabled)
        {
            engines.push(engineId)
        }
        else
        {
            const index = engines.indexOf(engineId)
            if (index !== -1)
                engines.splice(index, 1)
        }
        store.setUserEnabledAnalyticsEngines(engines)
    }
}
