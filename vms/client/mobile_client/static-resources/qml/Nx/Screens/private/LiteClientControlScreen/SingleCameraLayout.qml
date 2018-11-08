import QtQuick 2.6

Item
{
    property alias activeItem: cameraItem
    property alias layoutHelper: cameraItem.layoutHelper

    CameraItem
    {
        id: cameraItem

        anchors.fill: parent

        layoutX: layoutHelper.displayCell.x
        layoutY: layoutHelper.displayCell.y
    }
}
