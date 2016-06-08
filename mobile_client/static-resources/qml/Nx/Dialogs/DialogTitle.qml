import QtQuick 2.6
import Nx 1.0

Text
{
    width: parent ? parent.width : implicitWidth
    height: implicitHeight + topPadding + bottomPadding
    padding: 16
    font.pixelSize: 18
    font.weight: Font.DemiBold
    wrapMode: Text.WordWrap
    color: ColorTheme.base1
}
