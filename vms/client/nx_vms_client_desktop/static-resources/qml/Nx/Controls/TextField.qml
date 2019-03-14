import QtQuick 2.8
import QtQuick.Controls 2.0
import Nx 1.0

import "private"

TextField
{
    id: control

    implicitWidth: 200
    implicitHeight: 28
    leftPadding: 8
    rightPadding: 8

    font.pixelSize: 14

    color: enabled ? ColorTheme.text : ColorTheme.transparent(ColorTheme.text, 0.3)

    background: TextFieldBackground { control: parent }

    selectByMouse: true
    selectedTextColor: ColorTheme.brightText
    selectionColor: ColorTheme.highlight
}
