import QtQuick.Controls 2.0
import Nx 1.0

import "private"

ScrollView
{
    id: control

    property alias text: textArea.text

    implicitWidth: 200
    implicitHeight: 64

    TextArea
    {
        id: textArea

        leftPadding: 8
        rightPadding: control.ScrollBar.vertical.visible ? 12 : 8
        bottomPadding: control.ScrollBar.horizontal.visible ? 12 : 4

        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        font.pixelSize: 14

        color: enabled ? ColorTheme.text : ColorTheme.transparent(ColorTheme.text, 0.3)

        background: TextFieldBackground { control: parent }

        selectByMouse: true
        selectedTextColor: ColorTheme.brightText
        selectionColor: ColorTheme.highlight
    }
}
