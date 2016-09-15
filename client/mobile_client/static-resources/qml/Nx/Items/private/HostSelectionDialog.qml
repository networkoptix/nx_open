import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx.Dialogs 1.0
import Nx 1.0

DialogBase
{
    id: hostSelectionDialog

    property string activeHost: ""
    property alias hostsModel: hostsRepeater.model

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: true

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: qsTr("Hosts")
        }

        Repeater
        {
            id: hostsRepeater

            DialogListItem
            {
                id: control

                readonly property string host: model.display

                active: host == activeHost
                text: host

                onClicked:
                {
                    hostSelectionDialog.activeHost = host
                    hostSelectionDialog.close()
                }
            }
        }
    }
}
