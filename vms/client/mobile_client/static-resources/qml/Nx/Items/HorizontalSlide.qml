// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

/**
 * A reusable component for sliding its child items horizontally.
 * It can slide left or right and captures its children as an image before the slide to avoid
 * complex management of duplicate items.
 */
Item
{
    id: control

    property alias duration: slideAnimation.duration

    transform: Translate { x: d.xOffset }

    QtObject
    {
        id: d
        property real xOffset: 0
        property int direction: -1 //< -1 for left, 1 for right.

        NumberAnimation on xOffset
        {
            id: slideAnimation
            running: false
            duration: 200
            easing.type: Easing.InOutQuad
            from: -d.direction * control.width
            to: 0
        }
    }

    Image
    {
        id: image

        anchors.fill: parent
        source: ""
        visible: slideAnimation.running
        z: 1
        transform: Translate { x: d.direction * control.width }
    }

    // When current items are captured as an image and slide animation can start, items content can
    // be updated via updateFunc.
    function slide(direction, updateFunc)
    {
        d.direction = direction
        control.grabToImage(result => {
            image.source = result.url
            slideAnimation.start()
            updateFunc()
        })
    }

    function slideLeft(updateFunc)
    {
        slide(-1, updateFunc)
    }

    function slideRight(updateFunc)
    {
        slide(1, updateFunc)
    }
}
