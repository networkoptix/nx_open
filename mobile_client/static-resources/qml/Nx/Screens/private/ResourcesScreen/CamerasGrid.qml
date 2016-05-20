import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

GridView
{
    id: camerasGrid

    property real spacing: 8

    cellWidth: (width - leftMargin - rightMargin) / d.thumbnailsInRow
    cellHeight: cellWidth * 3 / 4 + 24 + 16

    QtObject
    {
        id: d

        readonly property real maxItemWidth: 192
        property int thumbnailsInRow: Math.max(2, Math.floor(camerasGrid.width / maxItemWidth))
    }

    model: QnCameraListModel
    {
        id: camerasModel
    }

    delegate: CameraItem
    {
        width: cellWidth
        height: cellHeight

        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail

        onClicked:
        {
            var point = mapToItem(camerasGrid.parent, width / 2, height / 2)
            Workflow.openVideoScreen(model.uuid, model.thumbnail, Math.max(0, point.x), Math.max(0, point.y))
        }

        onThumbnailRefreshRequested: camerasModel.refreshThumbnail(index)
    }
}
