import QtQuick 2.11
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Utils 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0
import Nx.InteractiveSettings 1.0
import Nx.Core 1.0

import nx.vms.client.core 1.0

import "private"

Item
{
    id: analyticsSettings

    property var store: null

    property var analyticsEngines: []
    property var enabledAnalyticsEngines: []

    property var currentEngineId
    property var currentEngineInfo
    property var currentSectionPath: []
    property var currentSettingsModel
    property bool loading: false
    property bool supportsDualStreaming: false

    readonly property bool isDeviceDependent: currentEngineInfo !== undefined
        && currentEngineInfo.isDeviceDependent

    readonly property bool isDefaultSection: currentEngineId !== undefined
        && currentSectionPath.length == 0

    Connections
    {
        target: store
        onStateChanged:
        {
            const resourceId = store.resourceId()

            loading = store.analyticsSettingsLoading()
            analyticsEngines = store.analyticsEngines()
            enabledAnalyticsEngines = store.enabledAnalyticsEngines()
            supportsDualStreaming = store.dualStreamingEnabled()
            if (settingsView.resourceId !== resourceId)
                settingsView.resourceId = resourceId
            mediaResourceHelper.resourceId = store.resourceId().toString()
            var engineId = store.currentAnalyticsEngineId()

            if (engineId === currentEngineId)
            {
                var actualModel = store.deviceAgentSettingsModel(currentEngineId)
                var actualValues = store.deviceAgentSettingsValues(currentEngineId)

                if (JSON.stringify(currentSettingsModel) == JSON.stringify(actualModel))
                {
                    settingsView.setValues(actualValues)
                }
                else
                {
                    currentSettingsModel = actualModel
                    settingsView.loadModel(currentSettingsModel, actualValues)
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

                navigationMenu.currentItemId =
                    [engineId.toString()].concat(currentSectionPath).join("\n")

                currentSettingsModel = store.deviceAgentSettingsModel(engineInfo.id)
                var actualValues = store.deviceAgentSettingsValues(engineInfo.id)

                settingsView.engineId = currentEngineId
                settingsView.loadModel(currentSettingsModel, actualValues)
            }
            else
            {
                currentEngineId = undefined
                currentEngineInfo = undefined
                currentSectionPath = []
                currentSettingsModel = undefined
                navigationMenu.currentItemId = undefined
                settingsView.loadModel({}, {})
            }

            if (currentEngineId)
                header.currentStreamIndex = store.analyticsStreamIndex(currentEngineId)

            updateCurrentSection()

            banner.visible = !store.recordingEnabled() && enabledAnalyticsEngines.length !== 0
        }
    }

    LiveThumbnailProvider
    {
        id: thumbnailProvider
        rotation: 0
        thumbnailsHeight: 200
    }

    MediaResourceHelper
    {
        id: mediaResourceHelper
    }

    NavigationMenu
    {
        id: navigationMenu

        width: 240
        height: parent.height - banner.height

        onItemClicked:
        {
            currentSectionPath = item.sections
            store.setCurrentAnalyticsEngineId(item.engineId)
            updateCurrentSection()
        }

        Repeater
        {
            model: analyticsEngines

            Rectangle
            {
                id: container

                width: parent.width
                height: column.height
                color: isSelected ? ColorTheme.colors.dark9 : "transparent"

                readonly property var engineId: modelData.id
                readonly property var isSelected: currentEngineId === engineId
                readonly property var settingsModel: isSelected ? currentSettingsModel : undefined

                Column
                {
                    id: column
                    width: parent.width

                    Loader
                    {
                        width: parent.width
                        sourceComponent: AnalyticsMenuItem {}

                        // Context properties:
                        property var ctx_navigationMenu: navigationMenu
                        property var ctx_modelData: modelData
                        property var ctx_engineId: engineId
                        property var ctx_settingsModel: settingsModel
                        property var ctx_sections: []
                        property int ctx_level: 0
                        property bool ctx_selected: container.isSelected
                        property bool ctx_active: enabledAnalyticsEngines.indexOf(engineId) !== -1
                            || analyticsEngines[index].isDeviceDependent
                    }

                    Rectangle
                    {
                        id: separator

                        width: parent.width
                        height: 1
                        color: ColorTheme.colors.dark7

                        Rectangle
                        {
                            y: 1
                            z: 1
                            width: parent.width
                            height: 1
                            color: ColorTheme.colors.dark9
                        }
                    }
                }
            }
        }
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

        onValuesEdited:
            store.setDeviceAgentSettingsValues(currentEngineId, getValues())

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
        store.setEnabledAnalyticsEngines(engines)
    }

    function updateCurrentSection()
    {
        var container = settingsView.contentItem.sectionsItem
        for (var i = 0; i < currentSectionPath.length; ++i)
        {
            var sectionIndex = currentSectionPath[i] + 1
            if (sectionIndex >= container.count)
            {
                currentSectionPath.length = i
                navigationMenu.currentItemId =
                    [currentEngineId.toString()].concat(currentSectionPath).join("\n")
                break
            }

            container.currentIndex = sectionIndex
            container = container.children[sectionIndex].sectionsItem
        }

        if (container)
            container.currentIndex = 0
    }
}
