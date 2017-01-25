import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Page
{
    id: settingsScreen

    title: qsTr("Settings")
    onLeftButtonClicked: Workflow.popCurrentScreen()
    topPadding: 24
    bottomPadding: 16

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

            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Live previews in the cameras list")
            }
            LabeledSwitch
            {
                width: parent.width
                text: qsTr("Save passwords for servers")
            }
        }
    }
}
