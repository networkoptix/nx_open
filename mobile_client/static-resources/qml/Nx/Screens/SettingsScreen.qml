import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0

Page
{
    id: settingsScreen

    title: qsTr("Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    padding: 16

    Flickable
    {
        id: flickable

        anchors.fill: parent

        contentWidth: width
        contentHeight: content.height

        Column
        {
            id: content

            width: flickable.width

            CheckBox
            {
                width: parent.width
                text: qsTr("Show offline cameras")
                checked: settings.showOfflineCameras
                onCheckedChanged: settings.showOfflineCameras = checked
            }
        }
    }
}
