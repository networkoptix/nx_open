import QtQuick 2.6
import Nx 1.0

Item
{
    id: control

    readonly property int kFontSize: 24

    property alias hours: hoursText.text
    property alias minutes: minutesText.text
    property string seconds
    property string suffix

    property color color: ColorTheme.windowText

    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    Row
    {
        id: contentRow

        Text
        {
            id: hoursText

            height: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: kFontSize
            font.weight: Font.Bold
            color: control.color
        }

        Text
        {
            text: ":"

            width: 8
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: kFontSize
            font.weight: Font.Light
            color: control.color
        }

        Text
        {
            id: minutesText

            height: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: kFontSize
            font.weight: Font.Normal
            color: control.color
        }

        Text
        {
            text: ":"

            width: 8
            height: 28

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: kFontSize
            font.weight: Font.Light
            color: control.color
        }

        Text
        {
            text: suffix.length
                  ? "%1 %2".arg(control.seconds).arg(control.suffix)
                  : control.seconds

            height: 28
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignHCenter

            font.pixelSize: kFontSize
            font.weight: Font.Light
            color: control.color
        }
    }
}

