import QtQuick 2.6
import Qt.labs.templates 1.0 as T
import Nx 1.0

T.Menu
{
    id: control

    implicitWidth: contentItem ? contentItem.implicitWidth + leftPadding + rightPadding : 0
    implicitHeight: contentItem ? contentItem.implicitHeight + topPadding + bottomPadding : 0

    property int orientation: Qt.Vertical

    background: Rectangle { color: ColorTheme.contrast3 }

    modal: true
    // TODO: #dklychkov Enable after switching to Qt 5.7
    // dim: false

    contentItem: ListView
    {
        orientation: control.orientation

        implicitWidth:
        {
            if (T.ApplicationWindow.window)
            {
                return Math.max(
                    orientation === Qt.Vertical ? 120 : 0,
                    Math.min(contentWidth, T.ApplicationWindow.window.width - 56))
            }
            return contentItem.childrenRect.width
        }
        implicitHeight:
        {
            if (T.ApplicationWindow.window)
            {
                return Math.max(
                    orientation === Qt.Horizontal ? 48 : 0,
                    Math.min(contentHeight, T.ApplicationWindow.window.height - 56))
            }
            return contentHeight
        }

        model: control.contentModel

        interactive: T.ApplicationWindow.window
            && contentHeight > T.ApplicationWindow.window.height

        clip: true
        keyNavigationWraps: false
        currentIndex: -1

        ScrollIndicator.vertical: ScrollIndicator {}
    }
}
