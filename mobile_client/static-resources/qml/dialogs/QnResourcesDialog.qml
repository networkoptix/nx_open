import QtQuick 2.2

Item {
    id: resourcesDialog

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        anchors.fill: parent
        color: __syspal.window
    }
}
