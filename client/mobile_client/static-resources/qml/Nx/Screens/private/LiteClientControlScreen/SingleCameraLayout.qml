import QtQuick 2.6

Item
{
    property CameraItem activeItem: cameraItem

    CameraItem
    {
        id: cameraItem

        anchors.fill: parent
    }
}
