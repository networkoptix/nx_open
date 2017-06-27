import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import com.networkoptix.qml 1.0

Rectangle
{
    id: cameraItem

    color: ColorTheme.base3

    property alias resourceId: resourceHelper.resourceId
    property alias status: resourceHelper.resourceStatus
    property alias resourceName: resourceHelper.resourceName
    property LiteClientLayoutHelper layoutHelper: null

    property bool useDummyText: true
    property int layoutX: -1
    property int layoutY: -1

    QtObject
    {
        id: d

        property bool offline: status == QnCameraListModel.Offline ||
                               status == QnCameraListModel.NotDefined ||
                               status == QnCameraListModel.Unauthorized
        property bool unauthorized: status == QnCameraListModel.Unauthorized
    }

    MediaResourceHelper
    {
        id: resourceHelper
    }

    QnThumbnailCacheAccessor
    {
        id: thumbnailCacheAccessor
        resourceId: cameraItem.resourceId
    }

    Loader
    {
        id: contentLoader

        anchors.centerIn: parent
        sourceComponent:
        {
            if (!resourceId)
                return noCameraComponent

            if (d.offline || d.unauthorized)
                return thumbnailDummyComponent

            if (!thumbnailCacheAccessor.thumbnailId)
                return thumbnailPreloaderComponent

            return thumbnailComponent
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
            }
        }
    }

    Component
    {
        id: thumbnailPreloaderComponent

        ThreeDotBusyIndicator {}
    }

    Component
    {
        id: thumbnailComponent

        Image
        {
            source: thumbnailCacheAccessor.thumbnailUrl
            width: cameraItem.width
            height: cameraItem.height
            fillMode: Image.PreserveAspectFit
        }
    }

    Component
    {
        id: noCameraComponent

        Text
        {
            visible: useDummyText
            width: cameraItem.width - 32
            text: qsTr("Select camera")
            color: ColorTheme.base13
            font.pixelSize: 24
            font.capitalization: Font.AllUppercase
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    Timer
    {
        id: refreshTimer

        interval: 60 * 1000
        repeat: true
        running: connectionManager.connectionState === QnConnectionManager.Ready
        triggeredOnStart: true

        onTriggered: thumbnailCacheAccessor.refreshThumbnail()
    }

    Connections
    {
        target: layoutHelper
        onCameraIdChanged:
        {
            if (layoutX == x && layoutY == y)
                cameraItem.resourceId = resourceId
        }
    }

    onLayoutHelperChanged: updateResourceId()
    Component.onCompleted: updateResourceId()

    function updateResourceId()
    {
        if (layoutX < 0 || layoutY < 0 || !layoutHelper)
            return

        resourceId = layoutHelper.cameraIdOnCell(layoutX, layoutY)
    }
}
