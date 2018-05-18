import QtQuick 2.6
import Nx 1.0

Item
{
    id: timeLabel

    property date dateTime
    property color color: ColorTheme.windowText

    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    readonly property var _locale: Qt.locale()
    readonly property int _fontSize: 24

    Row
    {
        id: contentRow

        Text
        {
            text: dateTime.toLocaleTimeString(_locale, "hh")

            width: 32
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: _fontSize
            font.weight: Font.Bold
            color: timeLabel.color
        }

        Text
        {
            text: ":"

            width: 8
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: _fontSize
            font.weight: Font.Light
            color: timeLabel.color
        }

        Text
        {
            text: dateTime.toLocaleTimeString(_locale, "mm")

            width: 32
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: _fontSize
            font.weight: Font.Normal
            color: timeLabel.color
        }

        Text
        {
            text: ":"

            width: 8
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: _fontSize
            font.weight: Font.Light
            color: timeLabel.color
        }

        Text
        {
            text: dateTime.toLocaleTimeString(_locale, "ss")

            width: 32
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: _fontSize
            font.weight: Font.Light
            color: timeLabel.color
        }
    }
}

