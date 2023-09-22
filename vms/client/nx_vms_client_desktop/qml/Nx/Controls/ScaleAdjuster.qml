// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQml 2.14

/**
 * Downscales contentItem if it doesn't fit.
 * Similar to QnViewportBoundWidget.
 */
Item
{
    id: adjuster

    property Item contentItem

    /**
     * The item contentItem must provide implicitWidth and implicitHeight, it will be kept at that
     * size and uniformly downscaled to fit if necessary.
     *
     * When resizeToContent is true, the control adjusts its size to contentItem implicit size;
     * maximumWidth and maximumHeight must be set externally.
     *
     * When resizeToContent is false, the control should be resized externally by settings its
     * width and height; those width and height will be considered maximumWidth and maximumHeight.
     */
    property bool resizeToContent: false

    property real maximumWidth: 0
    property real maximumHeight: 0

    implicitWidth: contentItem ? contentItem.implicitWidth : 0
    implicitHeight: contentItem ? contentItem.implicitHeight : 0

    // Set adjuster size to content size when resizeToContent is true.

    Binding
    {
        target: adjuster
        when: resizeToContent && contentItem
        property: "width"
        value: contentItem.width * contentItem.scale
        restoreMode: Binding.RestoreBindingOrValue
    }

    Binding
    {
        target: adjuster
        when: resizeToContent && contentItem
        property: "height"
        value: contentItem.height * contentItem.scale
        restoreMode: Binding.RestoreBindingOrValue
    }

    // Set maximum size to adjuster size when resizeToContent is false.

    Binding
    {
        target: adjuster
        when: !resizeToContent
        property: "maximumWidth"
        value: width
        restoreMode: Binding.RestoreBindingOrValue
    }

    Binding
    {
        target: adjuster
        when: !resizeToContent
        property: "maximumHeight"
        value: height
        restoreMode: Binding.RestoreBindingOrValue
    }

    // Setup content item bindings.

    Binding
    {
        target: contentItem
        property: "scale"
        value:
        {
            let result = 1.0
            if (contentItem.implicitWidth > 0 && adjuster.maximumWidth > 0)
                result = Math.min(result, adjuster.maximumWidth / contentItem.implicitWidth)

            if (contentItem.implicitHeight > 0 && adjuster.maximumHeight > 0)
                result = Math.min(result, adjuster.maximumHeight / contentItem.implicitHeight)

            return result
        }
    }

    Binding
    {
        target: contentItem
        property: "transformOrigin"
        value: Item.TopLeft
    }

    Binding
    {
        target: contentItem
        property: "width"
        value: Math.min(contentItem.implicitWidth, adjuster.maximumWidth / contentItem.scale)
    }

    Binding
    {
        target: contentItem
        property: "height"
        value: Math.min(contentItem.implicitHeight, adjuster.maximumHeight / contentItem.scale)
    }

    Binding
    {
        target: contentItem
        property: "parent"
        value: adjuster
    }

    Binding
    {
        target: contentItem
        property: "x"
        value: Math.round((adjuster.width - contentItem.width * contentItem.scale) / 2)
        when: !resizeToContent
    }

    Binding
    {
        target: contentItem
        property: "y"
        value: Math.round((adjuster.height - contentItem.height * contentItem.scale) / 2)
        when: !resizeToContent
    }
}
