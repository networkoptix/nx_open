// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx.Core 1.0

Popup
{
    id: control

    parent: mainWindow.contentItem

    property string result
    property bool disableAutoClose: false
    default property alias data: content.data

    width: Math.min(328, parent ? parent.width - 32 : 0)
    height: parent ? parent.height : 0
    x: parent && width > 0 ? (parent.width - width) / 2 : 0
    y: 0
    padding: 0
    modal: true
    closePolicy: disableAutoClose
        ? Popup.NoAutoClose
        : Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside

    background: null

    Overlay.modal: Rectangle
    {
        color: ColorTheme.colors.backgroundDimColor
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

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

                color: ColorTheme.colors.dark7
            }

            onWidthChanged: ensureActiveItemVisible()
            onHeightChanged: ensureActiveItemVisible()
        }

        onClicked:
        {
            if (!disableAutoClose)
                close()
        }
    }

    readonly property int _animationDuration: 200

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
        if (visible)
        {
            // Workaround for property 'opened' an signal name 'opened' conflict.
            NxGlobals.invokeMethod(control, "opened")
            return;
        }

        control.closed()
    }

    onOpened: contentItem.forceActiveFocus()

    onActiveItemChanged: ensureActiveItemVisible()

    function ensureActiveItemVisible()
    {
        if (activeItem)
            NxGlobals.ensureFlickableChildVisible(activeItem)
    }
}
