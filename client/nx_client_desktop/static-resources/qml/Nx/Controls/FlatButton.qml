import QtQuick 2.6
import Nx 1.0

Button
{
    id: control

    flat: true

    background: null

    textColor: ColorTheme.windowText
    textHoveredColor: ColorTheme.lighter(textColor, 2)

    leftPadding: (!text || (icon && iconAlignment !== Qt.AlignRight)) ? 0 : 2
    rightPadding: (!text || (icon && iconAlignment === Qt.AlignRight)) ? 0 : 2
    topPadding: 1
    bottomPadding: 1

    font.weight: Font.Normal

    iconSpacing: 2
    minimumIconWidth: 20

    implicitHeight: Math.max(20, label.implicitHeight + topPadding + bottomPadding)
}
