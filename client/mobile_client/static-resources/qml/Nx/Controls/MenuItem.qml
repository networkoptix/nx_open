import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

MenuItem
{
    id: control

    font.pixelSize: 16
    leftPadding: 16
    rightPadding: 16

    implicitWidth: Math.max(background.implicitWidth,
                            label.implicitWidth
                                + (indicator ? indicator.implicitWidth + spacing : 0)
                                + leftPadding + rightPadding)
    implicitHeight: Math.max(background.implicitHeight,
                             label.implicitHeight + topPadding + bottomPadding)

    background: Rectangle
    {
        implicitHeight: 48
        implicitWidth: 120
        color: ColorTheme.contrast3

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: control
            highlightColor: "#30000000"
        }
    }

    label: Text
    {
        text: control.text
        x: control.width - width - control.rightPadding
        y: control.topPadding
        width: control.availableWidth - (control.checkable ? indicator.width + control.spacing : 0)
        height: control.availableHeight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
        font: control.font
        color: control.enabled ? ColorTheme.base4
                               : ColorTheme.base6
    }
}
