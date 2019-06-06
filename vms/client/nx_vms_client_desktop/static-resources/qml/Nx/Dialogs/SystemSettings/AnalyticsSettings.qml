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
    readonly property alias currentEngineId: menu.currentItemId

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

        onAnalyticsEnginesChanged:
        {
            activateEngine(null)
        }
    }

    function activateEngine(engineId)
    {
        menu.currentItemId = engineId
        settingsView.loadModel(store.settingsModel(engineId), store.settingsValues(engineId))
    }

    onCurrentEngineIdChanged:
    {
        // Workaround for META-183.
        // TODO: #dklychkov Fix properly.
        // For some reason the notify signal of currentEngineId is not caught by C++ for the very
        // first time. However any handler on QML side fixes it. This is why this empty handler is
        // here.
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
