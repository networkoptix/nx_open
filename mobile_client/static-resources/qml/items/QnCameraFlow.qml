import QtQuick 2.4
import QtQuick.Controls 1.2
import QtQuick.Window 2.2

import Nx 1.0
import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"
import ".."

Item {
    id: cameraFlow

    property alias model: cameraGrid.model

    property bool animationsEnabled: true

    clip: true

    Object
    {
        id: d

        property var hiddenItems: []
        property real maxItemWidth: dp(192)
        property int thumbnailsInRow: Math.max(2, Math.floor(cameraGrid.width / maxItemWidth))

        function showHiddenItems()
        {
            if (hiddenItems.length == 0)
                return

            // use temporary object to update QML property correctly
            var items = settings.hiddenCameras.slice()

            for (var i = 0; i < hiddenItems.length; ++i)
            {
                var index = items.indexOf(hiddenItems[i])
                    if (index != -1)
                        items.splice(index, 1)
            }

            hiddenItems = []

            settings.hiddenCameras = items
        }

        Connections {
            target: resourcesPage
            onPageStatusChanged:
            {
                if (pageStatus != Stack.Active && pageStatus != Stack.Activating)
                    d.hiddenItems = []
            }
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
                    var items = d.hiddenItems

                    if (hidden)
                        items.push(model.uuid)
                    d.hiddenItems = items

                    settings.hiddenCameras.push(model.uuid)
                    hiddenCamerasPopup.restartTimer()
                }
            }

            Timer {
                id: refreshTimer

                readonly property int initialLoadDelay: 400
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
            enabled: animationsEnabled
            NumberAnimation { properties: "x,y"; duration: 250; easing.type: Easing.OutCubic }
        }
        displaced: Transition {
            enabled: animationsEnabled
            NumberAnimation { properties: "x,y"; duration: 250; easing.type: Easing.OutCubic }
        }
        add: Transition {
            id: addTransition
            enabled: animationsEnabled
            NumberAnimation { property: "y"; from: addTransition.ViewTransition.destination.y + dp(56); duration: 250; easing.type: Easing.OutCubic }
        }
        populate: Transition {
            id: populateTransition
            enabled: animationsEnabled
            NumberAnimation { property: "y"; from: populateTransition.ViewTransition.destination.y + dp(56); duration: 250; easing.type: Easing.OutCubic }
        }
        remove: Transition {
            enabled: animationsEnabled
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
            id: hiddenCameras

            property bool collapsed: true
            readonly property int collapseDuration: 250

            width: cameraFlow.width
            height: (hiddenCamerasList.count > 0) ? hiddenCamerasContent.height : 0

            Column {
                id: hiddenCamerasContent

                width: parent.width
                visible: hiddenCamerasList.count > 0

                Item {
                    height: dp(24)
                    width: parent.width

                    Rectangle {
                        anchors.verticalCenter: parent.bottom
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
                        rotation: hiddenCameras.collapsed ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: hiddenCameras.collapseDuration; easing.type: Easing.OutCubic } }
                        scale: iconScale()
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: dp(56)
                        text: qsTr("Hidden cameras")
                        font.pixelSize: sp(16)
                        font.weight: Font.DemiBold
                        color: QnTheme.listSectionText
                    }

                    QnMaterialSurface {
                        onClicked: {
                            hiddenCameras.collapsed = !hiddenCameras.collapsed
                            if (hiddenCameras.collapsed) {
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

                    SequentialAnimation {
                        id: expandAnimation

                        ParallelAnimation {
                            NumberAnimation {
                                target: hiddenList
                                property: "height"
                                to: hiddenCamerasColumn.height
                                duration: hiddenCameras.collapseDuration
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: cameraGrid
                                property: "contentY"
                                to: cameraGrid.contentY + Math.min(hiddenCamerasColumn.height, cameraGrid.height - dp(56))
                                duration: hiddenCameras.collapseDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        ScriptAction {
                            script: {
                                hiddenList.height = Qt.binding(function() { return hiddenCamerasColumn.height })
                                hiddenList.clip = false
                            }
                        }
                    }

                    SequentialAnimation {
                        id: collapseAnimation

                        ScriptAction {
                            script: {
                                hiddenList.height = hiddenList.height // kill the binding
                                hiddenList.clip = true
                            }
                        }

                        ParallelAnimation {
                            NumberAnimation {
                                target: hiddenList
                                property: "height"
                                to: 0
                                duration: hiddenCameras.collapseDuration
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    Column {
                        id: hiddenCamerasColumn

                        width: parent.width

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
                                    d.hiddenItems = []

                                    var items = settings.hiddenCameras.slice()

                                    var index = items.indexOf(model.uuid)
                                    if (index != -1)
                                        items.splice(index, 1)

                                    settings.hiddenCameras = items
                                }
                            }
                        }

                        move: Transition
                        {
                            enabled: animationsEnabled
                            NumberAnimation { properties: "y"; duration: 250; easing.type: Easing.OutCubic }
                        }
                        add: Transition
                        {
                            enabled: animationsEnabled
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 250; easing.type: Easing.OutCubic }
                        }
                    }
                }
            }
        }
    }

    QnToast {
        id: hiddenCamerasPopup

        property int prevCount: 0
        property int count: d.hiddenItems.length
        property bool isShown: count > 0

        timeout: 5000

        onCountChanged: {
            if (count > prevCount)
                text = qsTr("%n cameras are hidden", "", count)
        }

        mainButton {
            icon: "image://icon/undo.png"
            color: "transparent"
            onClicked: d.showHiddenItems()
        }

        onIsShownChanged: {
            if (isShown) {
                show()
            } else {
                hide()
                prevCount = 0
            }
        }

        onHidden: {
            d.hiddenItems = []
        }
    }
}
