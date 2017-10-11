import QtQuick 2.6;
import Nx.WelcomeScreen 1.0;

Item
{
    id: control;

    property alias textControl: captionControl;
    property alias background: backgroundControl;
    property int horizontalPadding: 12;

    implicitHeight: 32;
    implicitWidth: captionControl.implicitWidth + horizontalPadding * 2;

    Rectangle
    {
        id: backgroundControl;

        radius: 2;
        anchors.fill: parent;
        color: Style.custom.banner.warning.backgroundColor;
        opacity: Style.custom.banner.warning.backgroundOpacity;
    }

    NxLabel
    {
        id: captionControl;

        color: Style.custom.banner.warning.textColor;
        width: control.width - control.horizontalPadding;
        anchors.left: parent.left;
        anchors.leftMargin: control.horizontalPadding;
        anchors.verticalCenter: parent.verticalCenter;
    }

}
