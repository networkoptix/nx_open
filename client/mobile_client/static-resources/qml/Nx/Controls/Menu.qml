import QtQuick 2.6
import Qt.labs.templates 1.0 as T
import Nx 1.0

T.Menu
{
    id: control

    width: contentItem.implicitWidth + leftPadding + rightPadding
    height: contentItem.implicitHeight + topPadding + bottomPadding

    property int orientation: Qt.Vertical

    background: Rectangle { color: ColorTheme.contrast3 }

    modal: true
    // TODO: #dklychkov Enable after switching to Qt 5.7
    // dim: false

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
            if (T.ApplicationWindow.window)
            {
                return Math.max(
                    orientation === Qt.Vertical ? 120 : 0,
                    Math.min(
                        maxItemSize.width,
                        T.ApplicationWindow.window.width - 56))
            }
            return maxItemSize.width
        }
        implicitHeight:
        {
            if (T.ApplicationWindow.window)
            {
                return Math.max(
                    orientation === Qt.Horizontal ? 48 : 0,
                    Math.min(
                        listView.contentItem.childrenRect.height,
                        T.ApplicationWindow.window.height - 56))
            }
            return listView.contentItem.childrenRect.height
        }

        contentWidth: implicitWidth
        contentHeight: implicitHeight

        displayMarginBeginning: 256
        displayMarginEnd: 256

        model: control.contentModel

        interactive: T.ApplicationWindow.window
            && listView.contentHeight > T.ApplicationWindow.window.height

        clip: true
        keyNavigationWraps: false
        currentIndex: -1

        ScrollIndicator.vertical: ScrollIndicator {}

        onWidthChanged:
        {
            if (orientation !== Qt.Vertical)
                return

            for (var i = 0; i < listView.contentItem.children.length; ++i)
                listView.contentItem.children[i].width = width
        }
    }
}
