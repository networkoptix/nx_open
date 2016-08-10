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
            switchMode()
        }

        onDisplayModeChanged: switchMode()

        onCameraIdChanged:
        {
            var item = grid.itemAt(x, y)
            if (!item)
                return

            item.resourceId = resourceId
        }

        onSingleCameraIdChanged:
        {
            var item = stackView.currentItem
            if (item.objectName != "videoScreen")
                return

            item.resourceId = singleCameraId
        }

        function switchMode()
        {
            if (displayMode == QnLiteClientLayoutHelper.SingleCamera)
            {
                if (stackView.currentItem.objectName != "videoScreen")
                    Workflow.openVideoScreen(singleCameraId)
            }
            else
            {
                Workflow.openResourcesScreen()
            }
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

                property int layoutX: index % grid.columns
                property int layoutY: Math.floor(index / grid.columns)

                onClicked: layoutHelper.displayCell = Qt.point(layoutX, layoutY)
            }
        }

        function itemAt(x, y)
        {
            return repeater.itemAt(y * columns + x)
        }
    }

    Connections
    {
        target: resourcesScreen
        onActivePageChanged:
        {
            if (resourcesScreen.activePage)
                layoutHelper.displayCell = Qt.point(-1, -1)
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
