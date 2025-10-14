// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Item
{
    id: control

    property alias contentItem: content.contentItem
    property alias leftItem: leftControl.contentItem
    property alias rightItem: rightControl.contentItem

    property int revealDistance: 50
    property int activationDistance: 100

    property int spacing: 8

    property real shiftPosition: 0
    readonly property int shift: Math.round(shiftPosition)
    readonly property int shiftDirection: Math.sign(shift)

    readonly property var revealedControl:
    {
        if (Math.abs(shift) < revealDistance)
            return null
        if (shift > 0)
            return leftControl
        if (shift < 0)
            return rightControl
    }

    function activate()
    {
        if (revealedControl)
            revealedControl.contentItem.click()
    }

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    Connections
    {
        target: revealedControl && revealedControl.contentItem
        function onClicked(event) { activationAnimation.start() }
    }

    Control
    {
        id: content

        readonly property int maximum: (leftControl.implicitWidth + control.spacing)
        readonly property int minimum: -(rightControl.implicitWidth + control.spacing)

        function clamp(v, min, max)
        {
            return Math.max(min, Math.min(max, v));
        }

        x: clamp(shift, minimum, maximum)

        width: parent.width
        height: parent.height
    }

    Item //< Sticks to the content item.
    {
        anchors.right: content.left
        anchors.rightMargin: control.spacing

        width: leftControl.implicitWidth
        height: leftControl.implicitHeight

        Control //< Sticks to the container when stretching.
        {
            id: leftControl

            anchors.left: parent.left

            width: Math.max(implicitWidth, shift - control.spacing)
            height: control.height
        }
    }

    Item //< Sticks to the content item.
    {
        anchors.left: content.right
        anchors.leftMargin: control.spacing

        implicitWidth: rightControl.implicitWidth
        implicitHeight: rightControl.implicitHeight

        Control //< Sticks to the container when stretching.
        {
            id: rightControl

            anchors.right: parent.right

            width: Math.max(implicitWidth, -shift - control.spacing)
            height: control.height
        }
    }

    SequentialAnimation on shiftPosition
    {
        id: activationAnimation

        running: false

        NumberAnimation
        {
            to: revealedControl ? shiftDirection * (control.width + control.spacing) : 0
            duration: 600
            easing.type: Easing.OutCubic
        }

        NumberAnimation
        {
            to: 0
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    NumberAnimation on shiftPosition
    {
        id: defaultAnimation

        running: false
        duration: 300
        easing.type: Easing.OutCubic

        function run(reset)
        {
            to = reset || !revealedControl
                ? 0
                : shiftDirection * (revealedControl.implicitWidth + control.spacing)

            start();
        }
    }

    DragHandler
    {
        id: drag

        target: null
        enabled: !activationAnimation.running && !defaultAnimation.running
        dragThreshold: 20

        onActiveChanged:
        {
            if (!active && !activationAnimation.running)
                defaultAnimation.run()
        }

        yAxis.enabled: false
        xAxis.onActiveValueChanged: (delta) =>
        {
            shiftPosition += delta

            if (Math.abs(shift) > activationDistance)
                control.activate();
        }
    }

    MouseArea
    {
        anchors.fill: content
        enabled: !!revealedControl
        onClicked: defaultAnimation.run(/*reset*/ true)
    }

    layer.enabled: true
    layer.effect: OpacityMask
    {
        maskSource: Rectangle
        {
            width: control.width
            height: control.height
            radius: 10
        }
    }
}
