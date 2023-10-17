// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import nx.vms.client.desktop 1.0

/** Left panel wrapper for displaying a toolTip item inside native top-level transparent window. */
EmbeddedPopup
{
    id: toolTipPopup

    viewport: windowContext.mainWindow

    // Align either to top of the parent or using both parent and child vertical centers.
    property bool vcenter: true

    // Desired position of the tooltip without viewport constraints and margins.
    readonly property real desiredY:
    {
        if (!parent || !toolTip)
            return offsetY

        if (vcenter)
            return (parent.height - toolTip.implicitHeight) / 2 + offsetY

        return offsetY
    }

    x: parent ? parent.width : 0
    y: desiredY

    property real offsetY: 0
    property real viewportHeightLimit: 0

    // viewport.height is non-NOTIFYable property and QML prints warnings when some expression
    // depends on such property. Size changes are processed by EmbeddedPopup in C++ anyway, so the
    // following code only exists to silence QML warnings.
    property real viewportHeight: 0
    onViewportChanged:
    {
        if (viewport)
            viewportHeightLimit = viewportHeight = viewport.height
    }

    width: toolTip ? toolTip.implicitWidth : 0
    height: toolTip ? toolTip.implicitHeight : 0

    // Tab bar with for switching layouts is at the top of viewport.
    readonly property int viewportTabBarHeight: 24

    topMargin: viewportTabBarHeight + 30 //< + search textbox height
    bottomMargin: viewportHeight - viewportHeightLimit + 8

    visible: true //< Visibility is controlled via toolTip.visible

    default property var toolTip: null

    // This item goes into top-level window.
    Item
    {
        id: toolTipContainerItem

        width: toolTip ? toolTip.implicitWidth : 0
        height: toolTip ? toolTip.implicitHeight : 0

        // Re-parent toolTip inside container item since it will be displayed in another window.
        Binding
        {
            target: toolTip
            property: "parent"
            value: toolTipContainerItem
        }

        Binding
        {
            target: toolTip
            property: "pointerEdge"
            value: Qt.LeftEdge
        }

        // Setup position of tooltip pointer tip.
        Binding
        {
            target: toolTip
            property: "pointToY"
            value:
            {
                const pointerMargin = 16
                const offset = toolTipPopup.desiredY - toolTipPopup.y
                const yValue = offset + (toolTipPopup.vcenter
                    ? toolTip.implicitHeight / 2
                    : pointerMargin)

                // Avoid moving the tip too close to the edge.
                return clamp(pointerMargin, yValue, toolTip.implicitHeight - pointerMargin)
            }

            function clamp(min, v, max) { return Math.min(Math.max(v, min), max) }
        }
    }
}
