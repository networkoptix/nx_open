import QtQuick 2.11
import QtQuick.Controls 2.0

import Nx 1.0

import nx.client.desktop 1.0

CheckBox
{
    id: control

    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 20
    rightPadding: 0

    font.pixelSize: 13
    font.weight: Font.Normal

    baselineOffset: text.baselineOffset

    indicator: Image
    {
        y: topPadding
        opacity: enabled ? 1.0 : 0.3

        source:
        {
            var source = "qrc:///skin/theme/checkbox"
            if (control.checked)
                source += "_checked"
            if (control.hovered)
                source += "_hover"
            return source + ".png"
        }
    }

    contentItem: Text
    {
        id: text

        leftPadding: 2
        rightPadding: 2
        verticalAlignment: Text.AlignVCenter
        elide: Qt.ElideRight
        font: control.font
        text: control.text
        opacity: enabled ? 1.0 : 0.3

        color:
        {
            if (control.checked)
            {
                return (control.hovered && !control.pressed)
                    ? ColorTheme.brightText
                    : ColorTheme.text
            }

            return (control.hovered && !control.pressed)
                ? ColorTheme.lighter(ColorTheme.light, 2)
                : ColorTheme.light
        }
    }

    FocusFrame
    {
        anchors.fill: control.text ? contentItem : indicator
        anchors.margins: control.text ? 1 : 0
        visible: control.visualFocus
        color: ColorTheme.highlight
        opacity: 0.5
    }
}
