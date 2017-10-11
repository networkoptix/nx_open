import QtQuick 2.5;
import Nx.WelcomeScreen 1.0;

NxLabel
{
    id: thisComponent;

    property string linkCaption;
    property string link;

    autoHoverable: true;
    linkColor: color;
    standardColor: Style.colors.custom.banner.link;
    hoveredColor: Style.colors.custom.banner.linkHovered;
    hoveredCursorShape: Qt.PointingHandCursor;
    text: ("<a href=\"%2\">%1</a>").arg(linkCaption)
        .arg(link ? link : "_some_placeholder_to_make_link_working");
}
