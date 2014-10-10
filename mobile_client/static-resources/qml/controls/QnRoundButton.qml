import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtGraphicalEffects 1.0

import "../common_functions.js" as CommonFunctions

Button {
    id: button

    property color color: "#303030"
    property color pressColor: Qt.darker(color, 1.2)
    property color hoverColor: Qt.lighter(color, 1.2)
    property color iconColor: "#ffffff"
    property bool shadowEnabled: true
    property real zdepth: 1.0
    property string icon
    property int iconSize: -1

    width: CommonFunctions.dp(56)
    height: CommonFunctions.dp(56)

    style: ButtonStyle {
        background: Item {
            id: background

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
                    color: {
                        if (control.pressed)
                            return pressColor
                        else if (control.hovered)
                            return hoverColor
                        else
                            return control.color
                    }

                    Behavior on color { ColorAnimation { duration: 100 } }
                }

                visible: false
            }

            DropShadow {
                id: shadow
                anchors.fill: bg
                cached: true
                samples: Math.max(32, Math.round(radius * 2))
                source: bg

                spread: 0
                horizontalOffset: 0
                verticalOffset: CommonFunctions.dp(CommonFunctions.interpolate(1.5, 8, CommonFunctions.perc(1, 5, Math.min(zdepth, 5))))
                radius: CommonFunctions.dp(CommonFunctions.interpolate(1.5, 19, CommonFunctions.perc(1, 5, Math.min(zdepth, 5))))
                color: Qt.rgba(0, 0, 0, CommonFunctions.interpolate(0.12, 0.30, CommonFunctions.perc(1, 5, Math.min(zdepth, 5))))
            }

            DropShadow {
                id: bottomShadow
                anchors.fill: bg
                cached: true
                samples: Math.max(32, Math.round(radius * 2))
                source: bg

                spread: 0
                horizontalOffset: 0
                verticalOffset: CommonFunctions.dp(CommonFunctions.interpolate(1, 6, CommonFunctions.perc(1, 5, Math.min(zdepth, 5))))
                radius: CommonFunctions.dp(CommonFunctions.interpolate(1.2, 6, CommonFunctions.perc(1, 5, Math.min(zdepth, 5))))
                color: Qt.rgba(0, 0, 0, 0.12)
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
