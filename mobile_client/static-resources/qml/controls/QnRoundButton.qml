import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtGraphicalEffects 1.0

import "../common_functions.js" as CommonFunctions

Button {
    property color color: "#303030"
    property color iconColor: "#ffffff"
    property color shadowColor: "#80000000"
    property bool shadowEnabled: true
    property string icon
    property int iconSize: -1

    width: CommonFunctions.dp(64)
    height: CommonFunctions.dp(64)

    style: ButtonStyle {
        background: Item {
            Item {
                id: bg
                anchors.centerIn: parent
                width: control.width + shadow.radius * 2
                height: control.height + shadow.radius * 2

                Rectangle {
                    id: rectangle
                    anchors.fill: parent
                    anchors.margins: shadow.radius
                    width: control.width
                    height: control.height
                    radius: height / 2
                    color: control.color
                }

                visible: false
            }

            DropShadow {
                id: shadow
                anchors.fill: bg
                cached: true
                horizontalOffset: 0
                verticalOffset: 1
                radius: 6
                samples: 16
                color: control.shadowColor
                source: bg
            }
        }
        label: Item {
            Image {
                id: image
                anchors.centerIn: parent
                sourceSize.width: control.iconSize > 0 ? control.iconSize : undefined
                sourceSize.height: control.iconSize > 0 ? control.iconSize : undefined
                source: control.icon
                fillMode: Qt.KeepAspectRatio
                smooth: true

            }

            ColorOverlay {
                anchors.fill: image
                source: image
                cached: true
                color: control.iconColor
            }
        }
    }
}
