import QtQuick 2.6

Item
{
    property alias activeItem: cameraItem

    CameraItem
    {
        id: cameraItem

        anchors.fill: parent
    }
}
