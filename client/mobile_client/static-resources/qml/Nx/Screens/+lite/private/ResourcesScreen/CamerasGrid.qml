import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

Item
{
    id: camerasGrid

    property alias layoutId: layoutHelper.layoutId

    QnLiteClientLayoutHelper
    {
        id: layoutHelper

        onLayoutChanged:
        {
            resetLayout()

            if (displayMode == QnLiteClientLayoutHelper.SingleCamera)
                Workflow.openVideoScreen(Qt.binding(function() { return singleCameraId }))
        }

        onDisplayModeChanged:
        {
            if (displayMode == QnLiteClientLayoutHelper.SingleCamera)
                Workflow.openVideoScreen(Qt.binding(function() { return singleCameraId }))
            else
                Workflow.openResourcesScreen()
        }

        onCameraIdChanged:
        {
            var item = grid.itemAt(x, y)
            if (!item)
                return

            item.resourceId = resourceId
        }
    }

    Grid
    {
        id: grid

        anchors.fill: parent

        columns: 2
        rows: 2

        Repeater
        {
            id: repeater

            model: grid.columns * grid.rows

            CameraItem
            {
                width: grid.width / grid.columns
                height: grid.height / grid.rows
                paused: !resourcesScreen.activePage
            }
        }

        function itemAt(x, y)
        {
            return repeater.itemAt(y * columns + x)
        }
    }

    Component.onCompleted:
    {
        resetLayout()
        forceActiveFocus()
    }

    function resetLayout()
    {
        for (var y = 0; y < grid.rows; ++y)
        {
            for (var x = 0; x < grid.columns; ++x)
            {
                var item = grid.itemAt(x, y)
                item.resourceId = layoutHelper.cameraIdOnCell(x, y)
            }
        }
    }
}
