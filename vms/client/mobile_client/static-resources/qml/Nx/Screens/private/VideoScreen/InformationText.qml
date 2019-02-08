import QtQuick 2.6
import Nx 1.0

Text
{
    color: ColorTheme.transparent(ColorTheme.windowText, 0.8)
    font.pixelSize: 12
    width: Math.min(implicitWidth, 120)
    elide: Text.ElideRight
}
