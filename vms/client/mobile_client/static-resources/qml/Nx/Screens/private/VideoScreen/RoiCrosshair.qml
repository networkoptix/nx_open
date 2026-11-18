// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Qt5Compat.GraphicalEffects

import Nx.Core.Items

Item
{
    id: item

    property color color
    property color shadowColor
    property point centerPoint
    property bool expanded: false

    readonly property bool expandingFinished:
        expanded && d.circleRadius == d.kExpandedCircleRadius

    // Applies the translucency to the crosshair and its shadow as a whole.
    layer.enabled: true

    x: centerPoint.x - width / 2
    y: centerPoint.y - height / 2

    width: d.size + shadow.radius * 2
    height: width

    opacity: 0
    visible: opacity > 0

    Item
    {
        id: crosshair

        anchors.fill: parent
        visible: false

        Rectangle
        {
            anchors.verticalCenter: parent.verticalCenter
            x: d.thickness
            color: item.color
            width: d.dashSize + d.thickness
            height: d.thickness
        }

        Rectangle
        {
            anchors.verticalCenter: parent.verticalCenter
            x: parent.width - width - d.thickness
            color: item.color
            width: d.dashSize + d.thickness
            height: d.thickness
        }

        Rectangle
        {
            anchors.horizontalCenter: parent.horizontalCenter
            y: d.thickness
            color: item.color
            width: d.thickness
            height: d.dashSize + d.thickness
        }

        Rectangle
        {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height - height - d.thickness
            color: item.color
            width: d.thickness
            height: d.dashSize + d.thickness
        }

        Circle
        {
            anchors.centerIn: parent
            borderColor: item.color
            radius: d.circleRadius
            border.width: d.thickness
        }

        Circle
        {
            anchors.centerIn: parent
            circleColor: item.color
            radius: 2.5
            visible: item.expanded
        }
    }

    DropShadow
    {
        id: shadow

        anchors.fill: crosshair
        radius: 1
        spread: 1
        color: item.shadowColor
        source: crosshair
    }

    states: State
    {
        name: "expanded"
        when: item.expanded

        PropertyChanges
        {
            item.opacity: 0.4

            d.size: d.kExpandedSize
            d.thickness: 3
            d.circleRadius: d.kExpandedCircleRadius
            d.dashSize: 24
        }
    }

    Behavior on opacity
    {
        NumberAnimation { duration: d.kAnimationDuration; easing.type: Easing.OutCubic }
    }

    QtObject
    {
        id: d

        readonly property real kDashSize: 12
        readonly property real kCircleRadius: 5.5
        readonly property real kExpandedSize: 91
        readonly property real kExpandedCircleRadius: kExpandedSize / 2
        readonly property int kAnimationDuration: 400

        // Animated from the collapsed values to the expanded ones and back.
        property real dashSize: kDashSize
        property real circleRadius: kCircleRadius
        property real size: (kDashSize + kCircleRadius) * 2
        property real thickness: 1

        Behavior on size
        {
            NumberAnimation { duration: d.kAnimationDuration; easing.type: Easing.OutCubic }
        }

        Behavior on thickness
        {
            NumberAnimation { duration: d.kAnimationDuration; easing.type: Easing.OutCubic }
        }

        Behavior on circleRadius
        {
            NumberAnimation { duration: d.kAnimationDuration; easing.type: Easing.OutCubic }
        }

        Behavior on dashSize
        {
            NumberAnimation { duration: d.kAnimationDuration; easing.type: Easing.OutCubic }
        }
    }
}
