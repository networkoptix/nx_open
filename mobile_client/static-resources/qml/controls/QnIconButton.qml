import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2

import "../common_functions.js" as Common

Button {
    id: button

    property int iconSize: Common.dp(24)
    property string icon

    property bool __clipped: false

    width: iconSize
    height: iconSize

    style: ButtonStyle {
        background: Item {}

        label: Item {
            id: label
            Rectangle {
                id: roundClip
                anchors.centerIn: parent
                radius: width / 2
                color: "transparent"

                states: [
                    State {
                        name: "NORMAL"
                        when: !control.__clipped
                        PropertyChanges {
                            target: roundClip
                            width: label.width
                            height: label.height
                            clip: false
                        }
                    },
                    State {
                        name: "CLIPPED"
                        when: control.__clipped
                        PropertyChanges {
                            target: roundClip
                            width: 0
                            height: 0
                            clip: true
                        }
                    }
                ]

                transitions: [
                    Transition {
                        from: "NORMAL"
                        to: "CLIPPED"
                        SequentialAnimation {
                            PropertyAction { property: "clip" }
                            NumberAnimation { properties: "width,height"; easing.type: Easing.OutQuad; duration: 100 }
                        }
                    },
                    Transition {
                        from: "CLIPPED"
                        to: "NORMAL"
                        SequentialAnimation {
                            NumberAnimation { properties: "width,height"; easing.type: Easing.OutQuad; duration: 100 }
                            PropertyAction { property: "clip" }
                        }
                    }
                ]

                Image {
                    id: image
                    anchors.centerIn: parent
                    source: control.icon
                    sourceSize.height: control.iconSize
                    sourceSize.width: control.iconSize
                    fillMode: Qt.KeepAspectRatio
                }
            }
        }
    }
}
