import QtQuick 2.0
import Nx.Controls 1.0

ToolBarBase
{
    id: toolBar

    property alias leftButtonIcon: leftButton.icon
    property alias title: label.text
    property alias controls: controlsRow.data

    signal leftButtonClicked()

    IconButton
    {
        id: leftButton
        anchors.verticalCenter: parent.verticalCenter
        x: 8
        visible: icon != ""
        onClicked: leftButtonClicked()
    }

    PageTitleLabel
    {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: leftButton.visible ? 72 : 16
        width: titleControls.x - x - 8
    }

    Row
    {
        id: controlsRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 8
    }
}
