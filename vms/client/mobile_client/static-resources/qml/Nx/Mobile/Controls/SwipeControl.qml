// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import Nx.Core

Item
{
    id: control

    property alias contentItem: content.contentItem
    property alias leftItem: leftControl.contentItem
    property alias rightItem: rightControl.contentItem

    property int revealDistance: 50
    property int activationDistance: 100
    property var activationAnimation

    property int spacing: 8

    property real shiftPosition: 0
    readonly property int shift: Math.round(shiftPosition)
    readonly property int shiftDirection: Math.sign(shift)

    readonly property var revealedItem: revealedControl?.contentItem ?? null
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
        function onClicked(event)
        {
            if (activationAnimation)
                activationAnimation.start()
            else
                defaultAnimation.run(/*reset*/ true)
        }
    }

    Control
    {
        id: leftControl

        anchors.left: parent.left
        height: parent.height
        visible: shift > 0
    }

    Control
    {
        id: rightControl

        anchors.right: parent.right
        height: parent.height
        visible: shift < 0
    }


    Control
    {
        id: content

        readonly property int maximum: (leftControl.implicitWidth + control.spacing)
        readonly property int minimum: -(rightControl.implicitWidth + control.spacing)

        x: MathUtils.bound(minimum, shift, maximum)

        width: parent.width
        height: parent.height
    }

    MouseArea
    {
        anchors.fill: content
        enabled: !!revealedControl
        onClicked: defaultAnimation.run(/*reset*/ true)
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
        enabled: !defaultAnimation.running && !activationAnimation?.running
        dragThreshold: 20

        onActiveChanged:
        {
            if (active)
                return

            if (Math.abs(shift) > activationDistance)
                return control.activate()

            if (!defaultAnimation.running && !activationAnimation?.running)
                defaultAnimation.run()
        }

        yAxis.enabled: false
        xAxis.onActiveValueChanged: (delta) =>
        {
            const min = rightItem.opacity > 0 ? -(control.width + control.spacing) : 0
            const max = leftItem.opacity > 0 ? (control.width + control.spacing) : 0

            shiftPosition = MathUtils.bound(min, shiftPosition + delta, max)
        }
    }
}
