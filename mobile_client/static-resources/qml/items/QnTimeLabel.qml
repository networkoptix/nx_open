import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: timeLabel

    property date dateTime

    readonly property var _locale: Qt.locale()

    height: sp(48)
    width: dp(96 + 64)

    property int _fontPixelSize: sp(36)

    Text {
        id: minutesIndicator
        text: dateTime.toLocaleTimeString(_locale, "mm")
        anchors.centerIn: parent
        font.pixelSize: sp(_fontPixelSize)
        color: QnTheme.windowText
        font.weight: Font.Normal
    }

    Text {
        id: secondsSplitter
        text: ":"
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: dp(36)

        font.pixelSize: sp(_fontPixelSize)
        color: QnTheme.windowText
        font.weight: Font.Light
    }

    Text {
        id: minutesSplitter
        text: ":"
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: dp(-36)

        font.pixelSize: sp(_fontPixelSize)
        color: QnTheme.windowText
        font.weight: Font.Normal
    }

    Text {
        id: secondsIndicator
        text: dateTime.toLocaleTimeString(_locale, "ss")
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: secondsSplitter.right
        anchors.leftMargin: dp(16)

        font.pixelSize: sp(_fontPixelSize)
        color: QnTheme.windowText
        font.weight: Font.Light
    }

    Text {
        id: hoursIndicator
        text: dateTime.toLocaleTimeString(_locale, "hh")
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: minutesSplitter.left
        anchors.rightMargin: dp(16)

        font.pixelSize: sp(_fontPixelSize)
        color: QnTheme.windowText
        font.weight: Font.DemiBold
    }
}

