import QtQuick 2.6
import Nx 1.0

Text
{
    width: parent ? parent.width : implicitWidth
    height: contentHeight + topPadding + bottomPadding
    padding: 16
    topPadding: 0
    font.pixelSize: 16
    wrapMode: Text.WordWrap
    color: ColorTheme.base1
}
