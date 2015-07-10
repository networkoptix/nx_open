import QtQuick 2.4

import "../controls"

QnPopupMenu {
    id: cameraMenu

    width: content.width + content.x * 2
    height: content.height

    property string currentQuality

    signal selectQuality()

    Column {
        id: content
        x: parent.padding

        QnPopupMenuItem {
            text: qsTr("Quality") + ": " + (currentQuality ? currentQuality : qsTr("Auto"))
            onClicked: {
                cameraMenu.close()
                cameraMenu.selectQuality()
            }
        }
    }
}

