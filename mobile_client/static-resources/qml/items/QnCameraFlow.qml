import QtQuick 2.4
import QtQuick.Controls 1.2

import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

QnFlickable {
    id: cameraFlow

    property alias model: repeater.model

    contentWidth: width
    contentHeight: content.height
    topMargin: dp(16)
    leftMargin: dp(16)
    rightMargin: dp(16)
    bottomMargin: dp(16)

    QtObject {
        id: d

        property var pendingHiddenItems: []

        function hidePendingItems() {
            if (pendingHiddenItems.length == 0)
                return

            settings.hiddenCameras = settings.hiddenCameras.concat(settings.hiddenCameras, pendingHiddenItems)
            pendingHiddenItems = []
        }
    }

    Column {
        id: content

        width: parent.width
        x: -cameraFlow.leftMargin

        Item {
            id: flowContent

            width: parent.width
            height: flow.height
            clip: true

            Flow {
                id: flow

                property bool loaded: false

                spacing: dp(16)
                width: parent.width - cameraFlow.leftMargin - cameraFlow.rightMargin
                x: cameraFlow.leftMargin

                Repeater {
                    id: repeater

                    model: QnCameraListModel {
                        id: camerasModel
                        showOffline: settings.showOfflineCameras
                        hiddenCameras: settings.hiddenCameras
                    }

                    QnCameraItem {
                        text: model.resourceName
                        status: model.resourceStatus
                        thumbnail: model.thumbnail
                        thumbnailWidth: flow.width / 2 - flow.spacing / 2

                        onClicked: Main.openMediaResource(model.uuid)
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
                }

                move: Transition {
                    NumberAnimation { properties: "x,y"; duration: 500; easing.type: Easing.OutCubic }
                }
                add: Transition {
                    id: addTransition
                    enabled: flow.loaded
                    SequentialAnimation {
                        ScriptAction {
                            script: {
                                console.log("depacited")
                                console.log(addTransition.ViewTransition.targetIndexes)
                                addTransition.ViewTransition.item.opacity = 0.0
                            }
                        }
                        PauseAnimation {
                            duration: 100
                        }
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; to: 1.0; duration: 500; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "y"; from: addTransition.ViewTransition.destination.y + dp(56); duration: 500; easing.type: Easing.OutCubic }
                        }
                        ScriptAction {
                            script: {
                                console.log("opacited")
                                addTransition.ViewTransition.item.opacity = 1.0
                            }
                        }
                    }
                }
            }

            Behavior on height {
                id: flowHeightBehavior
                enabled: flow.loaded
                NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
            }
        }

        Column {
            id: hiddenCamerasContent

            width: cameraFlow.width
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
                    rotation: settings.hiddenCamerasCollapsed ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                    scale: iconScale()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    x: dp(72)
                    text: qsTr("Hidden cameras")
                    font.pixelSize: sp(15)
                    font.weight: Font.DemiBold
                    color: QnTheme.listSectionText
                }

                QnMaterialSurface {
                    onClicked: settings.hiddenCamerasCollapsed = !settings.hiddenCamerasCollapsed
                }
            }

            Item {
                width: parent.width
                height: settings.hiddenCamerasCollapsed ? 0 : hiddenCamerasColumn.height
                clip: true

                Behavior on height { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

                Column {
                    id: hiddenCamerasColumn
                    width: parent.width - cameraFlow.leftMargin - cameraFlow.rightMargin
                    x: cameraFlow.leftMargin

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

                            onClicked: Main.openMediaResource(model.uuid)
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
                        SequentialAnimation {
                            ScriptAction {
                                script: {
                                    hiddenIntemAddTransition.ViewTransition.item.opacity = 0.0
                                }
                            }
                            PauseAnimation {
                                duration: 100
                            }
                            ParallelAnimation {
                                NumberAnimation { property: "opacity"; to: 1.0; duration: 500; easing.type: Easing.OutCubic }
                                NumberAnimation { property: "y"; from: hiddenIntemAddTransition.ViewTransition.destination.y + dp(56); duration: 500; easing.type: Easing.OutCubic }
                            }
                            ScriptAction {
                                script: {
                                    hiddenIntemAddTransition.ViewTransition.item.opacity = 1.0
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    QnToast {
        id: hiddenCamerasPopup

        property bool shown: d.pendingHiddenItems.length > 0

        text: qsTr("%n cameras hidden", "", d.pendingHiddenItems.length)
        mainButton {
            icon: "image://icon/done.png"
            color: "transparent"
            onClicked: d.hidePendingItems()
        }

        onShownChanged: {
            if (shown)
                open()
            else
                close()
        }

        Connections {
            target: resourcesPage
            onPageStatusChanged: {
                if (pageStatus != Stack.Active && pageStatus != Stack.Activating)
                    hiddenCamerasPopup.close()
            }
        }
    }

    Timer {
        id: refreshTimer

        interval: 2 * 60 * 1000
        repeat: true
        running: true

        triggeredOnStart: true

        onTriggered: {
            model.refreshThumbnails(0, repeater.count)
        }
    }

    onWidthChanged: updateLayout()

    onVisibleChanged: {
        if (!visible)
            d.hidePendingItems()
    }

    Timer {
        id: loadedTimer
        interval: 500
        onTriggered: flow.loaded = true
        repeat: false
        running: false
    }

    function setLoaded() {
        loadedTimer.running = true
        updateLayout()
    }

    function updateLayout() {
        model.updateLayout(resourcesPage.width, 3.0)
    }
}
