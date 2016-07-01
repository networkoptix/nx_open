import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

ListView
{
    id: camerasList

    spacing: 8

    model: QnCameraListModel {}

    delegate: CameraListItem
    {
        width: camerasList.width
        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail

        onClicked:
        {
            var point = mapToItem(camerasGrid.parent, width / 2, height / 2)
            Workflow.openVideoScreen(model.uuid, model.thumbnail, Math.max(0, point.x), Math.max(0, point.y))
        }
    }
}
