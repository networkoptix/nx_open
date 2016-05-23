import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

GridView
{
    id: camerasGrid

    property real spacing: 8

    cellWidth: width / 2
    cellHeight: height / 2

    flow: GridView.FlowTopToBottom
    flickableDirection: Flickable.HorizontalFlick
    snapMode: GridView.SnapOneRow

    model: QnCameraListModel { id: camerasModel }

    delegate: CameraItem
    {
        width: cellWidth
        height: cellHeight

        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail
        useVideo: index >= camerasGrid._firstVideoIndex &&
                  index <= camerasGrid._lastVideoIndex
        paused: !resourcesScreen.activePage

        onClicked:
        {
            var point = mapToItem(camerasGrid.parent, width / 2, height / 2)
            Workflow.openVideoScreen(model.uuid, model.thumbnail, Math.max(0, point.x), Math.max(0, point.y))
        }

        onThumbnailRefreshRequested: camerasModel.refreshThumbnail(index)
    }

    highlight: Rectangle
    {
        width: cellWidth
        height: cellHeight
        z: 2.0
        color: "transparent"
        border.color: ColorTheme.contrast5
        border.width: 4
    }

    Component.onCompleted: forceActiveFocus()

    Connections
    {
        target: connectionManager
        onInitialResourcesReceived:
        {
            camerasGrid.currentIndex = 0
        }
    }

    readonly property int firstVisibleIndex:
    {
        if (contentWidth < cellWidth && contentHeight < cellHeight)
            return -1

        return indexAt(contentX + cellWidth / 2, contentY + cellHeight / 2)
    }

    readonly property int lastVisibleIndex:
    {
        if (contentWidth < cellWidth && contentHeight < cellHeight)
            return -1

        var index = indexAt(contentX + width - cellWidth / 2,
                            contentY + height - cellHeight / 2)
        if (index >= 0)
            return index

        return (firstVisibleIndex >= 0) ? count - 1 : -1
    }

    property int _firstVideoIndex: -1
    property int _lastVideoIndex: -1
    onFirstVisibleIndexChanged: videoIndexesUpdateDelay.restart()
    onLastVisibleIndexChanged: videoIndexesUpdateDelay.restart()

    Timer
    {
        id: videoIndexesUpdateDelay
        interval: 500

        onTriggered:
        {
            camerasGrid._firstVideoIndex = camerasGrid.firstVisibleIndex
            camerasGrid._lastVideoIndex = camerasGrid.lastVisibleIndex
        }
    }
}
