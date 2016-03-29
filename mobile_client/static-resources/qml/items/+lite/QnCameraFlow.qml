import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Window 2.2

import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"
import ".."

FocusScope
{
    id: cameraFlow

    property alias model: cameraGrid.model

    property bool animationsEnabled: true

    QnGridView
    {
        id: cameraGrid

        anchors.fill: parent
        anchors.margins: spacing / 2

        focus: true

        readonly property real spacing: dp(8)

        model: QnCameraListModel
        {
            id: camerasModel
        }

        cellWidth: width / 2
        cellHeight: height / 2

        flow: GridView.FlowTopToBottom
        flickableDirection: Flickable.HorizontalFlick
        snapMode: GridView.SnapOneRow

        delegate: delegateComponent
        highlight: highlightComponent

        Component.onCompleted: forceActiveFocus()

        Keys.onReturnPressed:
        {
            if (!currentItem)
                return

            currentItem.open()
        }

        Connections
        {
            target: connectionManager
            onInitialResourcesReceived:
            {
                cameraGrid.currentIndex = 0
            }
        }

        property int firstVideoIndex: -1
        property int lastVideoIndex: -1
        onFirstVisibleIndexChanged: videoIndexesUpdateDelay.restart()
        onLastVisibleIndexChanged: videoIndexesUpdateDelay.restart()

        Timer
        {
            id: videoIndexesUpdateDelay
            interval: 500

            onTriggered:
            {
                cameraGrid.firstVideoIndex = cameraGrid.firstVisibleIndex
                cameraGrid.lastVideoIndex = cameraGrid.lastVisibleIndex
            }
        }
    }

    Component
    {
        id: highlightComponent

        Item
        {
            width: cameraGrid.cellWidth
            height: cameraGrid.cellHeight
            z: 2.0

            Rectangle
            {
                anchors.fill: parent

                color: "transparent"
                border.color: QnTheme.listSelectionBorder
                border.width: dp(4)
            }
        }
    }

    Component
    {
        id: delegateComponent

        Item
        {
            width: cameraGrid.cellWidth
            height: cameraGrid.cellHeight

            QnCameraItem
            {
                anchors.fill: parent
                anchors.margins: cameraGrid.spacing / 2

                resourceId: model.uuid
                text: model.resourceName
                status: model.resourceStatus
                thumbnail: model.thumbnail
                useVideo: index >= cameraGrid.firstVideoIndex &&
                          index <= cameraGrid.lastVideoIndex
                paused: !resourcesPage.activePage

                onClicked: open()

                onThumbnailRefreshRequested:
                {
                    camerasModel.refreshThumbnail(index)
                }
            }

            function open()
            {
                var point = mapToItem(stackView, width / 2, height / 2)
                Main.openMediaResource(
                            model.uuid,
                            Math.max(0, point.x), Math.max(0, point.y),
                            model.thumbnail)
            }
        }
    }
}
