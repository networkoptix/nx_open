import QtQuick 2.11
import Nx 1.0

Rectangle
{
    id: control

    property int horizontalPadding: 16

    enum Style { Info, Error }
    property int style: Banner.Style.Error

    property alias text: label.text

    implicitHeight: label.implicitHeight + 16
    implicitWidth: parent ? parent.width : 200

    color:
    {
        if (style === Banner.Style.Info)
            return ColorTheme.colors.brand_d6
        return "#61282B"
    }

    Text
    {
        id: label

        x: horizontalPadding
        anchors.fill: parent
        leftPadding: horizontalPadding
        rightPadding: horizontalPadding
        verticalAlignment: Text.AlignVCenter

        wrapMode: Text.WordWrap
        color: ColorTheme.text
        font.pixelSize: 13
    }
}
