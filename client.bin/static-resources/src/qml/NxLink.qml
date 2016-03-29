import QtQuick 2.5;

import "."

NxLabel
{
    id: thisComponent;

    property string linkCaption;
    property string link;

    isHovered: hoverArea.containsMouse;
    linkColor: color;
    standardColor: Style.colors.custom.banner.link;
    hoveredColor: Style.colors.custom.banner.linkHovered;

    text: ("<a href=\"%2\">%1</a>").arg(linkCaption)
        .arg(link ? link : "_some_placeholder_to_make_link_working");

    MouseArea
    {
        id: hoverArea;

        hoverEnabled: true;
        anchors.fill: parent;
        acceptedButtons: Qt.NoButton; // pass all clicks to parent
        cursorShape: (thisComponent.isHovered ? Qt.PointingHandCursor : Qt.ArrowCursor);
    }
}
