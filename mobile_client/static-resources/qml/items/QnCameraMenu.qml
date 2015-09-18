import QtQuick 2.4

import "../controls"

QnPopupMenu {
    id: cameraMenu

    width: content.width
    height: content.height

    property string currentQuality

    signal selectQuality()

    Column {
        id: content

        QnPopupMenuItem {
            text: qsTr("Quality") + ": " + (currentQuality ? currentQuality : qsTr("Auto"))
            onClicked: {
                cameraMenu.hide()
                cameraMenu.selectQuality()
            }
        }
    }
}

