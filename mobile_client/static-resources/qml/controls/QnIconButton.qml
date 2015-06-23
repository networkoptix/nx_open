import QtQuick 2.4

Item {
    id: iconButton

    property alias icon: image.source

    readonly property alias pressed: materialSurface.pressed

    signal clicked()

    width: dp(48)
    height: dp(48)

    QnMaterialSurface {
        id: materialSurface

        anchors.margins: -4
        onClicked: iconButton.clicked()
        centered: true
        backlight: false
        clip: false
    }

    Image {
        id: image
        anchors.centerIn: parent
    }
}
