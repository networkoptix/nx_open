import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

import "private"

MenuItem
{
    id: control

    font.pixelSize: 16
    leftPadding: 16
    rightPadding: 16
    spacing: 16

    implicitWidth: enabled
        ? Math.max(
            background.implicitWidth,
            contentItem.implicitWidth
                + (indicator && control.checkable ? indicator.implicitWidth + spacing : 0)
                + leftPadding + rightPadding)
        : 0
    implicitHeight: enabled
        ? Math.max(
            background.implicitHeight,
            contentItem.implicitHeight + topPadding + bottomPadding)
        : 0

    background: Rectangle
    {
        implicitHeight: 48
        implicitWidth: 48
        color: ColorTheme.contrast3

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: control
            highlightColor: "#30000000"
        }
    }

    contentItem: Text
    {
        text: control.text
        x: control.leftPadding
        verticalAlignment: Text.AlignVCenter
        width: control.availableWidth
            - (control.indicator && control.checkable
               ? indicator.width + control.spacing
               : 0)
        elide: Text.ElideRight
        font: control.font
        color: control.enabled ? ColorTheme.base4 : ColorTheme.base6
        visible: enabled
    }

    indicator: CheckIndicator
    {
        x: control.width - control.rightPadding - width
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        visible: control.checkable && enabled
        checked: control.checked
        color: control.enabled ? ColorTheme.base4 : ColorTheme.base6
    }
}
