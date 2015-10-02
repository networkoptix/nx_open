import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: timeLabel

    property date dateTime

    readonly property var _locale: Qt.locale()

    height: sp(48)
    width: dp(96 + 64)

    property int _fontPixelSize: sp(36)

    TextMetrics {
        id: spaceMetrics
        text: "9"
        font: minutesIndicator.font
    }

    Text {
        id: hoursIndicator
        text: dateTime.toLocaleTimeString(_locale, "hh")
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: minutesSplitter.left
        anchors.rightMargin: spaceMetrics.width

        font.pixelSize: _fontPixelSize
        color: QnTheme.windowText
        font.weight: Font.Bold
    }

    Text {
        id: minutesSplitter
        text: ":"
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: -spaceMetrics.width * 2 - width / 2

        font: minutesIndicator.font
        color: QnTheme.windowText
    }

    Text {
        id: minutesIndicator
        text: dateTime.toLocaleTimeString(_locale, "mm")
        anchors.centerIn: parent
        font.pixelSize: _fontPixelSize
        color: QnTheme.windowText
        font.weight: Font.DemiBold
    }

    Text {
        id: secondsSplitter
        text: ":"
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: spaceMetrics.width * 2 + width / 2

        font: secondsIndicator.font
        color: QnTheme.windowText
    }

    Text {
        id: secondsIndicator
        text: dateTime.toLocaleTimeString(_locale, "ss")
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: secondsSplitter.right
        anchors.leftMargin: spaceMetrics.width

        font.pixelSize: _fontPixelSize
        color: QnTheme.windowText
        font.weight: Font.Normal
    }
}

