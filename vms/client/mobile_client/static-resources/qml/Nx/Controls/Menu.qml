import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

Menu
{
    id: control

    width: contentItem.implicitWidth + leftPadding + rightPadding
    height: contentItem.implicitHeight + topPadding + bottomPadding

    property int orientation: Qt.Vertical

    background: Rectangle { color: ColorTheme.contrast3 }

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside
    modal: true
    dim: false

    contentItem: ListView
    {
        id: listView

        orientation: control.orientation

        property size maxItemSize:
        {
            var w = 0.0
            var h = 0.0

            for (var i = 0; i < listView.contentItem.children.length; ++i)
            {
                w = Math.max(w, listView.contentItem.children[i].implicitWidth)
                h = Math.max(h, listView.contentItem.children[i].implicitHeight)
            }

            return Qt.size(w, h)
        }

        implicitWidth:
        {
            if (ApplicationWindow.window)
            {
                return Math.max(
                    orientation === Qt.Vertical ? 120 : 0,
                    Math.min(
                        maxItemSize.width,
                        ApplicationWindow.window.width - 56))
            }
            return maxItemSize.width
        }
        implicitHeight:
        {
            if (ApplicationWindow.window)
            {
                return Math.max(
                    orientation === Qt.Horizontal ? 48 : 0,
                    Math.min(
                        listView.contentItem.childrenRect.height,
                        ApplicationWindow.window.height - 56))
            }
            return listView.contentItem.childrenRect.height
        }

        contentWidth: implicitWidth
        contentHeight: implicitHeight

        displayMarginBeginning: 256
        displayMarginEnd: 256

        model: control.contentModel

        interactive: ApplicationWindow.window
            && listView.contentHeight > ApplicationWindow.window.height

        clip: true
        keyNavigationWraps: false
        currentIndex: -1

        ScrollIndicator.vertical: ScrollIndicator {}

        onWidthChanged:
        {
            if (orientation !== Qt.Vertical)
                return

            for (var i = 0; i < control.count; ++i)
                control.itemAt(i).width = width
        }
    }
}
