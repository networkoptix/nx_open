import QtQuick 2.6;
import Qt.labs.controls 1.0;

import ".";

Row
{
    id: control;

    property alias pagesCount: pageIndicator.count;
    property alias currentPage: pageIndicator.currentIndex;

    height: 16;
    spacing: 4;

    onPagesCountChanged:
    {
        if (currentPage >= pagesCount)
            currentPage = pagesCount - 1;
    }

    NxImageButton
    {
        id: leftArrow;
        enabled: (currentPage > 0);
        onClicked: { --currentPage; }
        anchors.verticalCenter: parent.verticalCenter;
        width: 24;
        height: 18;

        standardIconUrl: "qrc:/skin/welcome_page/paginator_previous.png";
        hoveredIconUrl: "qrc:/skin/welcome_page/paginator_previous_hovered.png";
    }

    PageIndicator
    {
        id: pageIndicator;

        anchors.verticalCenter: parent.verticalCenter;
        spacing: 0;
        interactive: true;
        delegate: NxImageButton
        {
            width: 18;
            height: 18;

            property bool selected: (control.currentPage == index);
            readonly property string selectedUrl: "qrc:/skin/welcome_page/paginator_dot_selected.png";
            readonly property string hoveredUrl: "qrc:/skin/welcome_page/paginator_dot_hovered.png";
            readonly property string standardUrl: "qrc:/skin/welcome_page/paginator_dot.png";

            standardIconUrl: (selected ? selectedUrl : standardUrl);
            hoveredIconUrl: (selected ? selectedUrl : hoveredUrl)

            onClicked: (control.currentPage = index );
        }
    }

    NxImageButton
    {
        id: rightArraw
        enabled: (currentPage < (pagesCount - 1));
        anchors.verticalCenter: parent.verticalCenter;
        onClicked: { ++currentPage; }
        width: 24;
        height: 18;

        standardIconUrl: "qrc:/skin/welcome_page/paginator_next.png";
        hoveredIconUrl: "qrc:/skin/welcome_page/paginator_next_hovered.png";
    }

}
