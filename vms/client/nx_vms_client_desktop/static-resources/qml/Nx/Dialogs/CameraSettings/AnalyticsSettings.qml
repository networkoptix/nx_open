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
    property bool loading: false

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
            if (settingsView.resourceId !== resourceId)
                settingsView.resourceId = resourceId
            mediaResourceHelper.resourceId = store.resourceId().toString()
            var engineId = store.currentAnalyticsEngineId()

            if (engineId === currentEngineId)
            {
                settingsView.setValues(store.deviceAgentSettingsValues(currentEngineId))
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

                settingsView.loadModel(
                    engineInfo.settingsModel,
                    store.deviceAgentSettingsValues(engineInfo.id))
            }
            else
            {
                currentEngineId = undefined
                currentEngineInfo = undefined
                currentSectionPath = []
                navigationMenu.currentItemId = undefined
                settingsView.loadModel({}, {})
            }

            banner.visible = !store.recordingEnabled() && enabledAnalyticsEngines.length !== 0
        }
    }

    LiveThumbnailProvider
    {
        id: thumbnailProvider
        rotation: 0
        thumbnailsHeight: 80
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

            var container = settingsView.contentItem.sectionsItem
            for (var i = 0; i < currentSectionPath.length; ++i)
            {
                container.currentIndex = currentSectionPath[i] + 1
                container = container.children[container.currentIndex].sectionsItem;
            }

            if (container)
                container.currentIndex = 0
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

    ColumnLayout
    {
        x: navigationMenu.width + 16
        y: 16
        width: parent.width - x - 16
        height: parent.height - 16 - banner.height
        spacing: 16

        enabled: !loading

        RowLayout
        {
            spacing: 16
            visible: isDefaultSection && isDeviceDependent

            Image
            {
                source: "qrc:/skin/standard_icons/sp_message_box_information.png"
            }

            Text
            {
                wrapMode: Text.WordWrap
                color: ColorTheme.windowText
                font.pixelSize: 13
                font.weight: Font.Bold
                text: qsTr("This is the built-in functionality")
            }
        }

        SwitchButton
        {
            id: enableSwitch
            text: qsTr("Enable")
            Layout.preferredWidth: Math.max(implicitWidth, 120)
            visible: isDefaultSection && !isDeviceDependent

            Binding
            {
                target: enableSwitch
                property: "checked"
                value: enabledAnalyticsEngines.indexOf(currentEngineId) !== -1
                when: currentEngineId !== undefined
            }

            onClicked:
            {
                var engines = enabledAnalyticsEngines.slice(0)
                if (checked)
                {
                    engines.push(currentEngineId)
                }
                else
                {
                    const index = engines.indexOf(currentEngineId)
                    if (index !== -1)
                        engines.splice(index, 1)
                }
                store.setEnabledAnalyticsEngines(engines)
            }
        }

        SettingsView
        {
            id: settingsView

            Layout.fillHeight: true
            Layout.fillWidth: true

            onValuesEdited:
                store.setDeviceAgentSettingsValues(currentEngineId, getValues())

            contentEnabled: enableSwitch.checked
            scrollBarParent: scrollBarsParent
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
}
