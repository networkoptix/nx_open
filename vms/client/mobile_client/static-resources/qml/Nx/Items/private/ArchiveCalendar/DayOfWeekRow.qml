import QtQuick 2.0
import Qt.labs.calendar 1.0
import Nx 1.0

DayOfWeekRow
{
    id: control

    implicitHeight: 24
    implicitWidth: 48 * 7
    spacing: 0

    delegate: Text
    {
        text: model.shortName
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: locale.weekDays.indexOf(model.day) == -1 ? ColorTheme.red_d1 : ColorTheme.contrast16
    }
}
