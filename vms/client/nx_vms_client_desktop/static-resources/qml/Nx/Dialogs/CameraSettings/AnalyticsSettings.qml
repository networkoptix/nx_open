import QtQuick 2.11
import QtQuick.Layouts 1.11
import Nx 1.0
import Nx.Utils 1.0
import Nx.Controls 1.0
import Nx.Controls.NavigationMenu 1.0
import Nx.InteractiveSettings 1.0

Item
{
    id: analyticsSettings

    property var store: null

    property var analyticsEngines: []
    property var enabledAnalyticsEngines: []

    property var currentEngineId
    property var currentEngineInfo
    property bool loading: false

    readonly property bool isDeviceDependent: currentEngineInfo !== undefined
        && currentEngineInfo.isDeviceDependent

    Connections
    {
        target: store
        onStateChanged:
        {
            loading = store.analyticsSettingsLoading()
            analyticsEngines = store.analyticsEngines()
            enabledAnalyticsEngines = store.enabledAnalyticsEngines()
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
                menu.currentItemId = engineId
                settingsView.loadModel(
                    engineInfo.settingsModel,
                    store.deviceAgentSettingsValues(engineInfo.id))
            }
            else
            {
                currentEngineId = undefined
                currentEngineInfo = undefined
                menu.currentItemId = undefined
                settingsView.loadModel({}, {})
            }

            alertBar.visible = !store.recordingEnabled() && enabledAnalyticsEngines.length !== 0
        }
    }

    NavigationMenu
    {
        id: menu

        width: 240
        height: parent.height - alertBar.height

        Repeater
        {
            model: analyticsEngines

            MenuItem
            {
                itemId: modelData.id
                text: modelData.name
                active: enabledAnalyticsEngines.indexOf(modelData.id) !== -1
                onClicked: store.setCurrentAnalyticsEngineId(modelData.id)
            }
        }
    }

    ColumnLayout
    {
        x: menu.width + 16
        y: 16
        width: parent.width - x - 16
        height: parent.height - 16 - alertBar.height
        spacing: 16

        enabled: !loading

        RowLayout
        {
            visible: currentEngineId !== undefined && isDeviceDependent
            spacing: 16

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
                text: qsTr("This is the default built-in functionality")
            }
        }

        SwitchButton
        {
            id: enableSwitch
            text: qsTr("Enable")
            Layout.preferredWidth: Math.max(implicitWidth, 120)
            visible: currentEngineId !== undefined && !isDeviceDependent

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

            onValuesEdited: store.setDeviceAgentSettingsValues(currentEngineId, getValues())

            enabled: enableSwitch.checked
        }
    }

    AlertBar
    {
        id: alertBar

        height: visible ? implicitHeight : 0
        visible: false
        anchors.bottom: parent.bottom
        label.text: qsTr("Camera analytics will work only when camera is being viewed."
            + " Enable recording to make it work all the time.")
    }
}
