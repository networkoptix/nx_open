import QtQuick 2.6
import Nx.Controls 1.0

Item
{
    id: launchButtonItem

    signal buttonClicked()

    Button
    {
        anchors.centerIn: parent
        text: qsTr("Turn On")
        onClicked: launchButtonItem.buttonClicked()
    }
}
