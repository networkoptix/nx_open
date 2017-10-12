import Nx 1.0

NxLabel
{
    property string linkCaption
    property string link

    autoHoverable: true
    linkColor: color
    standardColor: ColorTheme.colors.blue12
    hoveredColor: ColorTheme.colors.blue10
    hoveredCursorShape: Qt.PointingHandCursor
    text: ("<a href=\"%2\">%1</a>").arg(linkCaption)
        .arg(link ? link : "_some_placeholder_to_make_link_working")
}
