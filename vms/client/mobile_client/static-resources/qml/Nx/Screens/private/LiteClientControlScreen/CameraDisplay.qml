import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Item
{
    id: cameraItem

    property string resourceId
    property string thumbnail
    property int status

    signal thumbnailRefreshRequested()
    signal clicked()

    implicitWidth: d.thumbnailAspectRatio > 0
        ? (implicitHeight - 6) * d.thumbnailAspectRatio + 6
        : 142
    implicitHeight: 82

    QtObject
    {
        id: d

        property bool offline: status == QnCameraListModel.Offline ||
                               status == QnCameraListModel.NotDefined ||
                               status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized

        property Item thumbnailItem: (thumbnailContentLoader.sourceComponent == thumbnailComponent)
            ? thumbnailContentLoader.item : null

        property real thumbnailAspectRatio:
        {
            if (!thumbnailItem)
                return -1
            var size = thumbnailItem.sourceSize
            return size.width / size.height
        }
    }

    Rectangle
    {
        id: thumbnailContainer

        anchors.fill: parent
        anchors.margins: 3
        color: d.offline ? ColorTheme.base7 : ColorTheme.base4

        Loader
        {
            id: thumbnailContentLoader

            anchors.centerIn: parent
            sourceComponent:
            {
                if (d.offline || d.unauthorized)
                    return thumbnailDummyComponent
                else if (!thumbnail)
                    return thumbnailPreloaderComponent
                else
                    return thumbnailComponent
            }
        }

        MaterialEffect
        {
            clip: true
            anchors.fill: parent
            mouseArea: mouseArea
            rippleSize: width * 2
        }

        MouseArea
        {
            id: mouseArea
            anchors.fill: parent
            onClicked: cameraItem.clicked()
        }
    }

    Component
    {
        id: thumbnailDummyComponent

        Column
        {
            width: parent ? parent.width : 0
            leftPadding: 8
            rightPadding: 8
            readonly property real availableWidth: width - leftPadding - rightPadding

            Image
            {
                anchors.horizontalCenter: parent.horizontalCenter
                source: d.unauthorized
                    ? lp("/images/camera_locked.png")
                    : lp("/images/camera_offline.png")
                sourceSize.width: 48
                sourceSize.height: 48
            }
        }
    }

    Component
    {
        id: thumbnailPreloaderComponent

        ThreeDotBusyIndicator
        {
            scale: 0.75
        }
    }

    Component
    {
        id: thumbnailComponent

        Image
        {
            source: thumbnail
            width: thumbnailContainer.width
            height: thumbnailContainer.height
            fillMode: Image.PreserveAspectFit
        }
    }

    Timer
    {
        id: refreshTimer

        readonly property int initialLoadDelay: 400
        readonly property int reloadDelay: 60 * 1000

        interval: initialLoadDelay
        repeat: true
        running: connectionManager.connectionState === QnConnectionManager.Ready

        onTriggered:
        {
            interval = reloadDelay
            cameraItem.thumbnailRefreshRequested()
        }

        onRunningChanged:
        {
            if (!running)
                interval = initialLoadDelay
        }
    }
}
