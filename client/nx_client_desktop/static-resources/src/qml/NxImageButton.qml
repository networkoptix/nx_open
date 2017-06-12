
import QtQuick 2.6;

Image
{
    id: control;

    property string standardIconUrl;
    property string hoveredIconUrl;
    property string pressedIconUrl;
    property string disabledIconUrl;

    property alias isHovered: mouseArea.containsMouse;

    property bool forcePressed: false;

    signal clicked();

    opacity: (enabled || disabledIconUrl.length ? 1 : 0.3);
    source:
    {
        if (!control.enabled && disabledIconUrl.length)
            return disabledIconUrl;
        else if ((mouseArea.pressed || control.forcePressed) && pressedIconUrl.length)
            return pressedIconUrl;
        else if (mouseArea.containsMouse && hoveredIconUrl.length)
            return hoveredIconUrl;
        else
            return standardIconUrl;
    }

    MouseArea
    {
        id: mouseArea;

        anchors.fill: parent;
        hoverEnabled: hoveredIconUrl.length;

        onClicked: control.clicked();
    }
}
