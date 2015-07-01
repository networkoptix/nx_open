import QtQuick 2.4

Item {
    id: iconButton

    property alias icon: image.source
    property alias explicitRadius: materialSurface.explicitRadius
    property alias backlight: materialSurface.backlight

    readonly property alias pressed: materialSurface.pressed

    signal clicked()

    width: dp(48)
    height: dp(48)

    QnMaterialSurface {
        id: materialSurface

        onClicked: iconButton.clicked()
        backlight: false
        circular: true
        centered: true
        clip: false
    }

    Image {
        id: image
        anchors.centerIn: parent
    }
}
