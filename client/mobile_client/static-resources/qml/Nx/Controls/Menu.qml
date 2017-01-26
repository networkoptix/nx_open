import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

Menu
{
    id: control

    background: Rectangle
    {
        implicitWidth: 120
        implicitHeight: 120
        color: ColorTheme.contrast3
    }
    modal: true
    // TODO: #dklychkov Enable after switching to Qt 5.7
    // dim: false

    Binding
    {
        target: contentItem
        property: "implicitWidth"
        value: Math.max(contentItem.contentItem.childrenRect.width,
                        ApplicationWindow.window ? ApplicationWindow.window.width - 56 : 0)
    }
}
