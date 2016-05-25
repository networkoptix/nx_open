import QtQuick 2.0

import "../icons"

Item
{
    id: button

    property real progress: 0.0
    property bool menuOpened: false

    signal clicked()

    width: dp(48)
    height: dp(48)

    QnMaterialSurface
    {
        anchors.margins: -4
        onClicked: button.clicked()
        centered: true
        backlight: false
        clip: false
    }

    QnMenuBackIcon
    {
        id: icon
        anchors.centerIn: parent
        menuSate: !menuOpened
        animationProgress: progress
    }

    onProgressChanged:
    {
        if (progress == 0.0)
            menuOpened = false
        else if (progress == 1.0)
            menuOpened = true
    }
}
