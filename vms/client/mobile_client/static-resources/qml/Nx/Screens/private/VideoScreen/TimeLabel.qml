import QtQuick 2.6
import Nx 1.0

Item
{
    id: timeLabel

    property date dateTime
    property color color: ColorTheme.windowText

    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    Row
    {
        id: contentRow

        Text
        {
            text: d.getHours(dateTime)

            height: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: d.fontSize
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

            font.pixelSize: d.fontSize
            font.weight: Font.Light
            color: timeLabel.color
        }

        Text
        {
            text: d.getMinutes(dateTime)

            height: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: d.fontSize
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

            font.pixelSize: d.fontSize
            font.weight: Font.Light
            color: timeLabel.color
        }

        Text
        {
            text: d.getSeconds(dateTime)

            height: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: d.fontSize
            font.weight: Font.Light
            color: timeLabel.color
        }
    }

    Object
    {
        id: d

        readonly property int fontSize: 24
        readonly property var locale: Qt.locale()

        function getHours(dateTime)
        {
            return getLocalizedHours(dateTime)
        }

        function getMinutes(dateTime)
        {
            return dateTime.toLocaleTimeString(locale, "mm")
        }

        function getSeconds(dateTime)
        {
            var seconds = dateTime.toLocaleTimeString(locale, "ss")
            return is24HoursTimeFormat
                ? seconds
                : "%1 %2".arg(seconds).arg(getHoursTimeFormatMark(dateTime))
        }
    }
}

