import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

import "../icons"

Button {
    id: button

    property real progress: 0.0

    property bool menuOpened: false

    style: ButtonStyle {
        background: Item {
            implicitWidth: dp(36)
            implicitHeight: dp(36)
        }

        label: Item {
            QnMenuBackIcon {
                id: icon
                anchors.centerIn: parent
                menuSate: !control._menuOpened
                animationProgress: control.progress
            }
        }
    }

    onProgressChanged: {
        if (progress == 0.0)
            menuOpened = false
        else if (progress == 1.0)
            menuOpened = true
    }
}
