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
    property bool loading: false

    Connections
    {
        target: store
        onStateChanged:
        {
            loading = store.analyticsSettingsLoading()
            analyticsEngines = store.analyticsEngines()
            enabledAnalyticsEngines = store.enabledAnalyticsEngines()

            if (store.currentAnalyticsEngineId() === currentEngineId)
            {
                settingsView.setValues(store.deviceAgentSettingsValues(currentEngineId))
            }
            else
            {
                currentEngineId = store.currentAnalyticsEngineId()

                for (var i = 0; i < analyticsEngines.length; ++i)
                {
                    const engineInfo = analyticsEngines[i]
                    if (engineInfo.id === currentEngineId)
                    {
                        settingsView.loadModel(
                            engineInfo.settingsModel,
                            store.deviceAgentSettingsValues(currentEngineId))
                    }
                }
            }

            alertBar.visible = !store.recordingEnabled() && enabledAnalyticsEngines.length !== 0
        }
    }

    NavigationMenu
    {
        id: menu

        width: 240
        height: parent.height

        Repeater
        {
            model: analyticsEngines

            MenuItem
            {
                text: modelData.name
                active: enabledAnalyticsEngines.indexOf(modelData.id) !== -1
                onClicked: store.setCurrentAnalyticsEngineId(modelData.id)
            }
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 16
        anchors.leftMargin: menu.width + 16
        spacing: 16

        enabled: !loading

        SwitchButton
        {
            id: enableSwitch
            text: qsTr("Enable")
            Layout.preferredWidth: Math.max(implicitWidth, 120)
            visible: menu.currentItemId

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

            width: parent.width
            Layout.fillHeight: true

            onValuesEdited: store.setDeviceAgentSettingsValues(currentEngineId, getValues())

            enabled: enableSwitch.checked
        }
    }

    AlertBar
    {
        id: alertBar

        visible: false
        anchors.bottom: parent.bottom
        label.text: qsTr("Camera analytics will work only when camera is being viewed. Enable recording to make it work all the time.")
    }

}
