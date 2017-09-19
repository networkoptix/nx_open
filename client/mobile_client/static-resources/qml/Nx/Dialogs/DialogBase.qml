import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0

Popup
{
    id: control

    parent: mainWindow.contentItem

    property bool deleteOnClose: false
    default property alias data: content.data

    width: Math.min(328, parent.width - 32)
    height: parent.height
    x: width > 0 ? (parent.width - width) / 2 : 0
    y: 0
    padding: 0
    modal: true
    closePolicy: Popup.OnEscape | Popup.OnPressOutside | Popup.OnReleaseOutside

    background: null

    // The dialog will try to keep this item visible (in the screen boundaries).
    property Item activeItem: null

    contentItem: MouseArea
    {
        anchors.fill: parent
        anchors.topMargin: deviceStatusBarHeight

        Flickable
        {
            id: flickable

            width: parent.width
            height: Math.min(parent.height, implicitHeight)
            anchors.centerIn: parent

            implicitWidth: content.implicitWidth
            implicitHeight: contentHeight

            contentWidth: width
            contentHeight: content.implicitHeight + 32

            contentY: 0

            Rectangle
            {
                id: content

                implicitWidth: childrenRect.width
                implicitHeight: childrenRect.height
                width: parent.width
                y: 16

                color: ColorTheme.contrast3
            }

            onWidthChanged: ensureActiveItemVisible()
            onHeightChanged: ensureActiveItemVisible()
        }

        onClicked: close()
    }

    readonly property int _animationDuration: 200

    signal closed()
    signal opened()

    enter: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            duration: _animationDuration
            easing.type: Easing.OutCubic
            to: 1.0
        }
    }

    exit: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            duration: _animationDuration
            easing.type: Easing.OutCubic
            to: 0.0
        }
    }

    onVisibleChanged:
    {
        if (!visible)
        {
            control.closed()

            if (deleteOnClose)
                destroy()
        }
        else
        {
            control.opened()
        }
    }

    onOpened: contentItem.forceActiveFocus()

    onActiveItemChanged: ensureActiveItemVisible()

    function ensureActiveItemVisible()
    {
        if (activeItem)
            Nx.ensureFlickableChildVisible(activeItem)
    }
}
