import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx.Dialogs 1.0
import Nx 1.0

DialogBase
{
    id: itemSelectionDialog

    property string title: ""
    property string activeItem: ""
    property int activeItemRow: -1
    property alias model: repeater.model

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: false

    signal itemChanged()

    Column
    {
        width: parent.width

        DialogTitle
        {
            text: itemSelectionDialog.title
        }

        Repeater
        {
            id: repeater

            DialogListItem
            {
                id: dialogItem

                readonly property int row: index
                readonly property string value: model.display

                active: value == activeItem
                text: value

                onClicked:
                {
                    itemSelectionDialog.activeItem = value
                    itemSelectionDialog.activeItemRow = row
                    itemSelectionDialog.itemChanged()
                    itemSelectionDialog.close()
                }
            }
        }
    }
}
