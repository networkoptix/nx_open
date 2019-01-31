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

    property var store
    property alias currentEngineId: menu.currentItemId

    NavigationMenu
    {
        id: menu

        width: 240
        height: parent.height

        Repeater
        {
            model: store && store.analyticsEngines

            MenuItem
            {
                itemId: modelData.id
                text: modelData.name
                onClicked: activateEngine(modelData.id)
            }
        }
    }

    Connections
    {
        target: analyticsSettings.store || null
        onSettingsValuesChanged:
        {
            if (engineId === currentEngineId)
                settingsView.setValues(store.settingsValues(engineId))
        }
    }

    function activateEngine(engineId)
    {
        currentEngineId = engineId
        settingsView.loadModel(store.settingsModel(engineId), store.settingsValues(engineId))
    }

    SettingsView
    {
        id: settingsView

        anchors.fill: parent
        anchors.margins: 16
        anchors.leftMargin: menu.width + 16

        onValuesEdited: store.setSettingsValues(currentEngineId, getValues())
    }
}
