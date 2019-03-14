import QtQuick 2.6
import Nx 1.0
import nx.client.desktop 1.0

Item
{
    property alias cameraId: statusHelper.cameraId

    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    RecordingStatusHelper
    {
        id: statusHelper
    }

    Image
    {
        id: image
        anchors.centerIn: parent
        source: statusHelper.qmlIconName
    }
}
