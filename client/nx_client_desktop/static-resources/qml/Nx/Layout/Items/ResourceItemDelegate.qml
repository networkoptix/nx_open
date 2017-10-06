import QtQuick 2.6
import Nx 1.0

Item
{
    property var modelData

    readonly property alias titleBar: titleBar

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.colors.dark5
    }

    TitleBar
    {
        id: titleBar
        titleText: modelData.resource ? modelData.resource.name : ""
    }
}
