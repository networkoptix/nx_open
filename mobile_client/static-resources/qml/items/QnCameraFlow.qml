import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../main.js" as Main

import "../controls"

Flickable {
    id: cameraFlow

    property alias model: repeater.model

    contentWidth: width
    contentHeight: content.height
    leftMargin: dp(16)
    rightMargin: dp(16)
    bottomMargin: dp(16)

    Column {
        id: content

        width: parent.width

        Item {
            id: flowContent
            width: parent.width
            height: flow.height
            clip: true

            Flow {
                id: flow

                spacing: dp(16)
                width: cameraFlow.width - cameraFlow.leftMargin - cameraFlow.rightMargin

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
                        onSwyped: {
                            settings.hiddenCameras.push(model.uuid)
                        }
                    }
                }

                move: Transition {
                    NumberAnimation { properties: "x,y"; duration: 500; easing.type: Easing.OutCubic }
                }
                add: Transition {
                    NumberAnimation { properties: "opacity"; duration: 500; easing.type: Easing.OutCubic }
                }

            }

            Behavior on height { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
        }

        Column {
            id: hiddenCamerasContent
            width: cameraFlow.width
            x: -cameraFlow.leftMargin

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
                    source: "qrc:///images/section_open.png"
                    anchors.verticalCenter: parent.verticalCenter
                    x: dp(16)
                    rotation: settings.hiddenCamerasCollapsed ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    x: dp(48)
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

                            onClicked: Main.openMediaResource(model.uuid)
                            onShowClicked: {
                                var originalList = settings.hiddenCameras

                                var hiddenCameras = []
                                for (var i = 0; i < originalList.length; i++) {
                                    if (originalList[i] !== model.uuid)
                                        hiddenCameras.push(originalList[i])
                                }

                                settings.hiddenCameras = hiddenCameras
                            }
                        }
                    }
                    move: Transition {
                        NumberAnimation { properties: "x"; duration: 500; easing.type: Easing.OutCubic }
                    }
                    add: Transition {
                        NumberAnimation { properties: "opacity"; duration: 500; easing.type: Easing.OutCubic }
                    }
                }
            }
        }
    }

//    Scrollbar { flickableItem: rootItem }

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

    function updateLayout() {
        model.updateLayout(resourcesPage.width, 3.0)
    }
}
