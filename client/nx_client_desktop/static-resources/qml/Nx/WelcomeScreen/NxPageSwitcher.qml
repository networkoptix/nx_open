import QtQuick 2.6;
import Qt.labs.controls 1.0;
import Nx.WelcomeScreen 1.0;

Row
{
    id: control;

    property alias pagesCount: pageIndicator.count;
    readonly property int page: impl.currentPage;

    height: 16;
    spacing: 4;

    signal currentPageChanged(int index, bool byClick);

    function setPage(index)
    {
        impl.updateCurrentPage(index, false);
    }

    onPagesCountChanged:
    {
        if (impl.currentPage >= pagesCount)
            impl.updateCurrentPage(pagesCount - 1, false);
        else if ((impl.currentPage < 0) && (pagesCount > 0))
            impl.updateCurrentPage(0, false);
    }

    NxImageButton
    {
        enabled: (impl.currentPage > 0);
        onClicked: { impl.updateCurrentPage(impl.currentPage - 1, true); }
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

            property bool selected: (impl.currentPage == index);
            readonly property string selectedUrl: "qrc:/skin/welcome_page/paginator_dot_selected.png";
            readonly property string hoveredUrl: "qrc:/skin/welcome_page/paginator_dot_hovered.png";
            readonly property string standardUrl: "qrc:/skin/welcome_page/paginator_dot.png";

            standardIconUrl: (selected ? selectedUrl : standardUrl);
            hoveredIconUrl: (selected ? selectedUrl : hoveredUrl)

            onClicked: (impl.updateCurrentPage(index, true));
        }
    }

    NxImageButton
    {
        enabled: (impl.currentPage < (pagesCount - 1));
        anchors.verticalCenter: parent.verticalCenter;
        onClicked: { impl.updateCurrentPage(impl.currentPage + 1, true); }
        width: 24;
        height: 18;

        standardIconUrl: "qrc:/skin/welcome_page/paginator_next.png";
        hoveredIconUrl: "qrc:/skin/welcome_page/paginator_next_hovered.png";
    }

    property QtObject impl: QtObject
    {
        property int currentPage: -1;

        function updateCurrentPage(index, byClick)
        {
            if (currentPage == index)
                return;

            currentPage = index;
            control.currentPageChanged(index, byClick);
        }
    }
}
