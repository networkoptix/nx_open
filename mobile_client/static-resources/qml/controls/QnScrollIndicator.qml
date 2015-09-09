import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: scrollIndicator

    property Flickable flickable

    anchors.fill: flickable

    Rectangle {
        id: indicator

        anchors {
            right: parent.right
            rightMargin: dp(6)
        }

        width: dp(4)
        radius: width / 2

        y: flickable.visibleArea.yPosition * scrollIndicator.height
        height: flickable.visibleArea.heightRatio * flickable.height

        color: QnTheme.calendarSplitter
        opacity: 0.0

        NumberAnimation on opacity {
            id: opacityAnimation
            duration : 250
            to: 0.0
        }

        function show() {
            if (height >= scrollIndicator.height)
                return

            hideTimer.stop()
            opacityAnimation.stop()
            indicator.opacity = 1.0
        }

        function hide() {
            hideTimer.start()
        }
    }

    Connections {
        target: flickable

        onMovingChanged: {
            if (flickable.moving)
                indicator.show()
            else
                indicator.hide()
        }
    }

    Timer {
        id: hideTimer

        repeat: false
        running: false
        interval: 1000
        onTriggered: opacityAnimation.start()
    }
}
