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

Item
{
    id: analyticsSettings

    property var store: null
    property var backend: null

    property var analyticsEngines: []
    property var enabledAnalyticsEngines: []

    property var currentEngineId
    property var currentEngineInfo
    property var currentSettingsModel
    property bool loading: false
    property bool supportsDualStreaming: false

    readonly property bool isDeviceDependent: currentEngineInfo !== undefined
        && currentEngineInfo.isDeviceDependent

    Connections
    {
        target: store

        function onStateChanged()
        {
            const resourceId = store.resourceId()
            if (resourceId.isNull())
                return

            loading = store.analyticsSettingsLoading()
            analyticsEngines = store.analyticsEngines()
            enabledAnalyticsEngines = store.userEnabledAnalyticsEngines()
            supportsDualStreaming = store.dualStreamingEnabled()
            let engineId = store.currentAnalyticsEngineId()

            const canUseSectionPath =
                (settingsView.resourceId === resourceId && engineId === currentEngineId)

            if (settingsView.resourceId !== resourceId)
                settingsView.resourceId = resourceId
            mediaResourceHelper.resourceId = resourceId

            if (engineId === currentEngineId)
            {
                const actualModel = settingsView.preprocessedModel(
                    store.deviceAgentSettingsModel(currentEngineId))
                const actualValues = store.deviceAgentSettingsValues(currentEngineId)

                if (JSON.stringify(currentSettingsModel) == JSON.stringify(actualModel))
                {
                    settingsView.setValues(actualValues)
                }
                else
                {
                    currentSettingsModel = actualModel
                    settingsView.loadModel(
                        currentSettingsModel, actualValues, /*restoreScrollPosition*/ true)

                    if (navigationMenu.lastClickedSectionId || !canUseSectionPath)
                    {
                        navigationMenu.currentSectionPath =
                            settingsView.sectionPath(navigationMenu.lastClickedSectionId)
                        navigationMenu.currentItemId =
                            navigationMenu.getItemId(engineId, navigationMenu.currentSectionPath)
                    }
                    settingsView.selectSection(navigationMenu.currentSectionPath)
                }
            }
            else if (analyticsEngines.length > 0)
            {
                var engineInfo = undefined
                for (var i = 0; i < analyticsEngines.length; ++i)
                {
                    var info = analyticsEngines[i]
                    if (info.id === engineId)
                    {
                        engineInfo = info
                        break
                    }
                }

                // Select first engine in the list if nothing selected.
                if (!engineInfo)
                {
                    engineInfo = analyticsEngines[0]
                    engineId = engineInfo.id
                }

                currentEngineId = engineId
                currentEngineInfo = engineInfo
                navigationMenu.currentEngineId = engineId
                navigationMenu.currentSectionPath =
                    settingsView.sectionPath(navigationMenu.lastClickedSectionId)
                navigationMenu.currentItemId =
                    navigationMenu.getItemId(engineId, navigationMenu.currentSectionPath)

                currentSettingsModel = settingsView.preprocessedModel(
                    store.deviceAgentSettingsModel(engineInfo.id))
                var actualValues = store.deviceAgentSettingsValues(engineInfo.id)

                settingsView.engineId = currentEngineId
                settingsView.loadModel(currentSettingsModel, actualValues)
                settingsView.selectSection(navigationMenu.currentSectionPath)
            }
            else
            {
                currentEngineId = undefined
                currentEngineInfo = undefined
                currentSettingsModel = undefined
                navigationMenu.currentItemId = undefined
                navigationMenu.currentSectionPath = []
                settingsView.loadModel({}, {})
            }

            settingsView.setErrors(store.deviceAgentSettingsErrors(engineId))

            if (currentEngineId)
                header.currentStreamIndex = store.analyticsStreamIndex(currentEngineId)

            banner.visible = !store.recordingEnabled() && enabledAnalyticsEngines.length !== 0
        }
    }

    MediaResourceHelper
    {
        id: mediaResourceHelper
    }

    AnalyticsSettingsMenu
    {
        id: navigationMenu

        width: 240
        height: parent.height - banner.height

        engines: analyticsEngines
        enabledEngines: enabledAnalyticsEngines
        currentEngineSettingsModel: currentSettingsModel

        onCurrentEngineIdChanged:
            store.setCurrentAnalyticsEngineId(currentEngineId)
        onCurrentSectionPathChanged:
            settingsView.selectSection(currentSectionPath)
    }

    SettingsView
    {
        id: settingsView

        x: navigationMenu.width + 16
        y: 16
        width: parent.width - x - 24
        height: parent.height - 16 - banner.height

        enabled: !loading
        contentEnabled: header.checked || isDeviceDependent
        contentVisible: contentEnabled
        scrollBarParent: scrollBarsParent

        onValuesEdited: function(activeItem)
        {
            let parameters = activeItem && activeItem.parametersModel
                ? backend.requestParameters(activeItem.parametersModel)
                : {}

            if (parameters == null)
                return

            store.setDeviceAgentSettingsValues(
                currentEngineId,
                activeItem ? activeItem.name : "",
                getValues(),
                parameters)
        }

        headerItem: InformationPanel
        {
            id: header

            checkable: !isDeviceDependent && !!currentEngineId
            checked: checkable && enabledAnalyticsEngines.indexOf(currentEngineId) !== -1
            refreshing: analyticsSettings.loading
            engineInfo: currentEngineInfo
            streamSelectorVisible: supportsDualStreaming && currentStreamIndex >= 0
            streamSelectorEnabled: settingsView.contentEnabled
            Layout.bottomMargin: 16
            Layout.fillWidth: true

            onEnableSwitchClicked:
            {
                if (currentEngineId)
                    setEngineEnabled(currentEngineId, !checked)
            }

            onRefreshButtonClicked:
            {
                if (currentEngineId)
                    store.refreshDeviceAgentSettings(currentEngineId)
            }

            onCurrentStreamIndexChanged:
            {
                if (currentEngineId)
                    store.setAnalyticsStreamIndex(currentEngineId, currentStreamIndex)
            }
        }

        placeholderItem: SettingsPlaceholder
        {
            header: qsTr("This plugin has no settings for this Camera.")
            description:
                qsTr("Check System Administration Settings to configure this plugin.")
        }
    }

    Item
    {
        id: scrollBarsParent
        width: parent.width
        height: banner.y
    }

    Banner
    {
        id: banner

        height: visible ? implicitHeight : 0
        visible: false
        anchors.bottom: parent.bottom
        text: qsTr("Camera analytics will work only when camera is being viewed."
            + " Enable recording to make it work all the time.")
    }

    function setEngineEnabled(engineId, enabled)
    {
        var engines = enabledAnalyticsEngines.slice(0)
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
