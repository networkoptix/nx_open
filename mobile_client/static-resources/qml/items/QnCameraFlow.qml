import QtQuick 2.4
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

Item {
    id: cameraFlow

    property alias model: cameraGrid.model

    clip: true

    QtObject {
        id: d

        property var pendingHiddenItems: []
        property real maxItemWidth: dp(192)
        property int thumbnailsInRow: Math.max(2, Math.floor(cameraGrid.width / maxItemWidth))

        function hidePendingItems() {
            if (pendingHiddenItems.length == 0)
                return

            settings.hiddenCameras = settings.hiddenCameras.concat(settings.hiddenCameras, pendingHiddenItems)
            pendingHiddenItems = []
        }
    }

    QnGridView {
        id: cameraGrid

        anchors.fill: parent

        readonly property real spacing: dp(8)

        topMargin: spacing
        bottomMargin: spacing

        model: QnCameraListModel {
            id: camerasModel
            showOffline: settings.showOfflineCameras
            hiddenCameras: settings.hiddenCameras
        }

        cellWidth: (width - spacing) / d.thumbnailsInRow
        cellHeight: cellWidth * 3 / 4 + dp(24) + dp(16)

        delegate: Item {
            width: cameraGrid.cellWidth
            height: cameraGrid.cellHeight

            QnCameraItem {
                anchors.right: parent.right
                width: parent.width - cameraGrid.spacing

                text: model.resourceName
                status: model.resourceStatus
                thumbnail: model.thumbnail

                onClicked: {
                    var point = mapToItem(stackView, width / 2, height / 2)
                    Main.openMediaResource(model.uuid, Math.max(0, point.x), Math.max(0, point.y), model.thumbnail)
                }
                onHiddenChanged: {
                    // use temporary object to update QML property correctly
                    var items = d.pendingHiddenItems

                    if (hidden) {
                        items.push(model.uuid)
                    } else {
                        var index = items.indexOf(model.uuid)
                        if (index != -1)
                            items.splice(index, 1)
                    }

                    d.pendingHiddenItems = items
                }
            }

            Timer {
                id: refreshTimer

                readonly property int initialLoadDelay: 1000
                readonly property int reloadDelay: 60 * 1000

                interval: initialLoadDelay
                repeat: true
                running: connectionManager.connectionState == QnConnectionManager.Connected

                onTriggered: {
                    interval = reloadDelay
                    camerasModel.refreshThumbnail(index)
                }

                onRunningChanged: {
                    if (!running)
                        interval = initialLoadDelay
                }
            }
        }

        move: Transition {
            NumberAnimation { properties: "x,y"; duration: 500; easing.type: Easing.OutCubic }
        }
        displaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 500; easing.type: Easing.OutCubic }
        }
        add: Transition {
            id: addTransition
            NumberAnimation { property: "y"; from: addTransition.ViewTransition.destination.y + dp(56); duration: 500; easing.type: Easing.OutCubic }
        }
        populate: Transition {
            id: populateTransition
            NumberAnimation { property: "y"; from: populateTransition.ViewTransition.destination.y + dp(56); duration: 500; easing.type: Easing.OutCubic }
        }
        remove: Transition {
            NumberAnimation { property: "opacity"; to: 0.0; duration: 250; easing.type: Easing.OutCubic }
        }

        footer: hiddenCamerasComponent
    }

    QnScrollIndicator {
        flickable: cameraGrid
    }

    Component {
        id: hiddenCamerasComponent

        Item {
            width: cameraFlow.width
            height: hiddenCamerasContent.height

            Column {
                id: hiddenCamerasContent

                property bool collapsed: true

                width: parent.width - cameraGrid.spacing * 2
                x: cameraGrid.spacing
                visible: hiddenCamerasList.count > 0

                Item {
                    height: dp(24)
                    width: parent.width

                    Rectangle {
                        anchors.verticalCenter: parent.bottom
                        anchors.verticalCenterOffset: -dp(8)
                        width: parent.width
                        height: dp(1)
                        color: QnTheme.listSeparator
                    }
                }

                Item {
                    width: parent.width
                    height: dp(48)

                    Image {
                        source: "image://icon/section_open.png"
                        anchors.verticalCenter: parent.verticalCenter
                        x: dp(16)
                        rotation: hiddenCamerasContent.collapsed ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                        scale: iconScale()
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: dp(72)
                        text: qsTr("Hidden cameras")
                        font.pixelSize: sp(16)
                        font.weight: Font.DemiBold
                        color: QnTheme.listSectionText
                    }

                    QnMaterialSurface {
                        onClicked: {
                            hiddenCamerasContent.collapsed = !hiddenCamerasContent.collapsed
                            if (hiddenCamerasContent.collapsed) {
                                expandAnimation.stop()
                                collapseAnimation.start()
                            } else {
                                collapseAnimation.stop()
                                expandAnimation.start()
                            }
                        }
                    }
                }

                Item {
                    id: hiddenList
                    width: parent.width
                    height: 0
                    clip: true

                    ParallelAnimation {
                        id: expandAnimation
                        NumberAnimation {
                            target: hiddenList
                            property: "height"
                            to: hiddenCamerasColumn.height
                            duration: 500
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: cameraGrid
                            property: "contentY"
                            to: cameraGrid.contentY + Math.min(hiddenCamerasColumn.height, cameraGrid.height - dp(56))
                            duration: 500
                            easing.type: Easing.OutCubic
                        }
                    }

                    ParallelAnimation {
                        id: collapseAnimation
                        NumberAnimation {
                            target: hiddenList
                            property: "height"
                            to: 0
                            duration: 500
                            easing.type: Easing.OutCubic
                        }
                    }

                    Column {
                        id: hiddenCamerasColumn
                        width: parent.width - cameraGrid.leftMargin - cameraGrid.rightMargin
                        x: cameraGrid.leftMargin

                        Repeater {
                            id: hiddenCamerasList
                            model: QnCameraListModel {
                                id: hiddenCamerasModel
                                showOffline: settings.showOfflineCameras
                                hiddenCameras: settings.hiddenCameras
                                hiddenCamerasOnly: true
                            }

                            QnHiddenCameraItem {
                                text: model.resourceName
                                status: model.resourceStatus

                                onClicked: {
                                    var point = mapToItem(stackView, width / 2, height / 2)
                                    Main.openMediaResource(model.uuid, Math.max(0, point.x), Math.max(0, point.y), model.thumbnail)
                                }
                                onShowClicked: {
                                    var originalList = settings.hiddenCameras

                                    var hiddenCameras = d.pendingHiddenItems
                                    d.pendingHiddenItems = []

                                    for (var i = 0; i < originalList.length; i++) {
                                        if (originalList[i] !== model.uuid)
                                            hiddenCameras.push(originalList[i])
                                    }

                                    settings.hiddenCameras = hiddenCameras
                                }
                            }
                        }
                        move: Transition {
                            NumberAnimation { properties: "y"; duration: 500; easing.type: Easing.OutCubic }
                        }
                        add: Transition {
                            id: hiddenIntemAddTransition
                            NumberAnimation { property: "y"; from: hiddenIntemAddTransition.ViewTransition.destination.y + dp(56); duration: 500; easing.type: Easing.OutCubic }
                        }
                    }
                }
            }
        }
    }

    QnToast {
        id: hiddenCamerasPopup

        property bool isShown: d.pendingHiddenItems.length > 0

        text: qsTr("%n cameras hidden", "", d.pendingHiddenItems.length)
        mainButton {
            icon: "image://icon/done.png"
            color: "transparent"
            onClicked: d.hidePendingItems()
        }

        onIsShownChanged: {
            if (isShown)
                show()
            else
                hide()
        }

        Connections {
            target: resourcesPage
            onPageStatusChanged: {
                if (pageStatus != Stack.Active && pageStatus != Stack.Activating)
                    hiddenCamerasPopup.hide()
            }
        }
    }

    onVisibleChanged: {
        if (!visible)
            d.hidePendingItems()
    }
}
