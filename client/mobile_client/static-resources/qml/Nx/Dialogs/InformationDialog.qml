import QtQuick 2.6
import Qt.labs.controls 1.0

DialogBase
{
    id: dialog

    property alias title: dialogTitle.text
    property alias message: dialogMessage.text
    property alias buttonsModel: buttonBox.buttonsModel

    signal buttonClicked(string buttonId)

    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside
    deleteOnClose: true

    contentItem: Column
    {
        width: parent.width

        DialogTitle
        {
            id: dialogTitle
            visible: text
        }
        DialogMessage
        {
            id: dialogMessage
            visible: text
            topPadding: dialogTitle.visible ? 0 : 16
        }

        DialogButtonBox
        {
            id: buttonBox
            visible: buttonsModel.length > 0
            onButtonClicked:
            {
                close()
                dialog.buttonClicked(buttonId)
            }
        }
    }
}
