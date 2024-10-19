// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx.Items 1.0
import Nx.Core 1.0

Popup
{
    id: control

    property alias chunkProvider: calendar.chunkProvider
    property alias position: calendar.position
    property alias displayOffset: calendar.displayOffset

    signal picked(real position)

    readonly property int _animationDuration: 200

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnReleaseOutside
    modal: true

    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight
    y: parent.height - implicitHeight
    padding: 0

    background: null

    Overlay.modal: Rectangle
    {
        color: ColorTheme.colors.backgroundDimColor
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    contentItem: ArchiveCalendar
    {
        id: calendar
        layer.enabled: true

        onPicked: control.picked(position)
        onCloseClicked: close()
    }

    enter: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                to: 1.0
            }

            NumberAnimation
            {
                target: contentItem
                property: "y"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                from: 56
                to: 0
            }
        }
    }

    exit: Transition
    {
        ParallelAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                to: 0.0
            }

            NumberAnimation
            {
                target: contentItem
                property: "y"
                duration: _animationDuration
                easing.type: Easing.OutCubic
                to: 56
            }
        }
    }

    onVisibleChanged:
    {
        // Workaround for property 'opened' an signal name 'opened' conflict.
        if (visible)
            NxGlobals.invokeMethod(control, "opened")
        else
            closed()
    }

    onOpened:
    {
        calendar.resetToCurrentPosition()
        contentItem.forceActiveFocus()
    }

    onPicked: close()
}
