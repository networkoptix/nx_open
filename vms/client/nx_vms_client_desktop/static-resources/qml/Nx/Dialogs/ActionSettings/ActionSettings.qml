import QtQuick 2.11
import Nx.InteractiveSettings 1.0

Item
{
    id: actionSettings

    function loadModel(model)
    {
        settingsView.loadModel(model)
    }

    function getValues()
    {
        return settingsView.getValues()
    }

    SettingsView
    {
        id: settingsView

        anchors.fill: parent
        anchors.margins: 16
    }
}
