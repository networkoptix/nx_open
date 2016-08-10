import QtQuick 2.6
import Nx 1.0
import com.networkoptix.qml 1.0

Item
{
    id: camerasGrid

    property alias layoutId: gridLayoutHelper.layoutId

    QnLiteClientLayoutHelper
    {
        id: gridLayoutHelper

        onLayoutChanged: switchMode()
        onDisplayModeChanged: switchMode()

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

    QnCameraListModel
    {
        id: camerasModel

        function nextResourceId(resourceId)
        {
            if (count == 0)
                return ""

            if (resourceId == "")
                return resourceIdByRow(0)

            var index = rowByResourceId(resourceId)
            if (index == count - 1)
                return ""

            return resourceIdByRow(index + 1)
        }
        function previousResourceId(resourceId)
        {
            if (count == 0)
                return ""

            if (resourceId == "")
                return resourceIdByRow(count - 1)

            var index = rowByResourceId(resourceId)
            if (index == 0)
                return ""

            return resourceIdByRow(index - 1)
        }
    }

    GridView
    {
        id: grid

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds

        property int columns: 2
        property int rows: 2
        model: rows * columns
        cellWidth: width / columns
        cellHeight: height / rows

        currentIndex: 0

        delegate: CameraItem
        {
            width: grid.cellWidth
            height: grid.cellHeight
            paused: !resourcesScreen.activePage
            active: GridView.isCurrentItem
            layoutHelper: gridLayoutHelper
            layoutX: index % grid.columns
            layoutY: Math.floor(index / grid.columns)

            onClicked: gridLayoutHelper.displayCell = Qt.point(layoutX, layoutY)
            onNextCameraRequested:
            {
                gridLayoutHelper.setCameraIdOnCell(
                    layoutX, layoutY, camerasModel.nextResourceId(resourceId))
            }
            onPreviousCameraRequested:
            {
                gridLayoutHelper.setCameraIdOnCell(
                    layoutX, layoutY, camerasModel.previousResourceId(resourceId))
            }
        }
    }

    Connections
    {
        target: resourcesScreen
        onActivePageChanged:
        {
            if (resourcesScreen.activePage)
                gridLayoutHelper.displayCell = Qt.point(-1, -1)
        }
    }

    Component.onCompleted: grid.forceActiveFocus()
}
