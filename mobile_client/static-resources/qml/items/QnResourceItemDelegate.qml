import QtQuick 2.2

Item {
    id: itemDelegate

    property string name
    property int type

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    height: text.height

    Text {
        id: text
        text: name
        renderType: Text.NativeRendering
        color: __syspal.windowText
    }
}
