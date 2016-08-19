import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Dialogs 1.0

DialogBase
{
    id: hostSelectionDialog

    property string activeHost: ""
    property alias hostsModel: hostsRepeater.model

    implicitHeight: Math.min(parent.height, contentItem.implicitHeight)

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: true

    background: null

    contentItem: Flickable
    {
        id: flickable

        anchors.fill: parent

        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height

        contentWidth: width
        contentHeight: column.height

        topMargin: 16 + getStatusBarHeight()
        bottomMargin: 16
        contentY: 0

        Rectangle
        {
            anchors.fill: column
            color: ColorTheme.contrast3
        }

        Column
        {
            id: column

            width: flickable.width

            DialogTitle
            {
                text: qsTr("Hosts")
                bottomPadding: 6
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
}
