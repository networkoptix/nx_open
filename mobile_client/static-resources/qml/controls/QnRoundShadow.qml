import QtQuick 2.2
import QtGraphicalEffects 1.0

import "../common_functions.js" as CommonFunctions

Item {
    property real shadowSize: CommonFunctions.dp(1)

    clip: false

    RadialGradient {
        width: parent.width + shadowSize + shadowSize
        height: parent.height + shadowSize + shadowSize
        x: -shadowSize
        y: shadowSize + shadowSize / 2

        horizontalRadius: width / 2
        verticalRadius: height / 2

        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 1) }
            GradientStop { position: 0.8; color: Qt.rgba(0, 0, 0, 0.4) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0) }
        }
    }
}
