import QtQuick 2.6
import Nx 1.0
import Nx.Core 1.0
import com.networkoptix.qml 1.0

Item
{
    id: camerasGrid

    property alias layoutId: gridLayoutHelper.layoutId

    LiteClientLayoutHelper
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
            if (!layoutId)
                return

            if (displayMode === LiteClientLayoutHelper.SingleCamera)
            {
                if (stackView.currentItem.objectName != "videoScreen")
                {
                    var item = Workflow.openVideoScreen(singleCameraId)
                    item.layoutHelper = gridLayoutHelper
                    item.camerasModel = camerasModel
                }
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

            onClicked: grid.currentIndex = index
            onDoubleClicked: gridLayoutHelper.displayCell = Qt.point(layoutX, layoutY)
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

            onActivityDetected:
            {
                if (grid.currentItem)
                    grid.currentItem.showControls()
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
