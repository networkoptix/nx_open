import QtQuick 2.6
import Nx 1.0

Text
{
    text: qsTr("You are not connected to any System")
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter
    lineHeight: 20
    lineHeightMode: Text.FixedHeight
    wrapMode: Text.WordWrap
    color: ColorTheme.base16
    font.pixelSize: 16
}
