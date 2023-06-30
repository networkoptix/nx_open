// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Control
{
    id: control

    property var layoutItemData
    property var resourceItem: null

    property Item overlayParent: control

    property bool rotationAllowed: false

    readonly property alias titleBar: titleBar

    readonly property real parentRotation: parent ? parent.rotation : 0

    background: Rectangle
    {
        color: ColorTheme.colors.dark5
    }

    Item
    {
        id: ui
        z: 10

        anchors.centerIn: parent

        rotation: -Math.round(parentRotation / 90) * 90

        parent: overlayParent || control

        width: rotation % 180 === 0 ? parent.width : parent.height
        height: rotation % 180 === 0 ? parent.height : parent.width

        TitleBar
        {
            id: titleBar
            titleText: layoutItemData.resource ? layoutItemData.resource.name : ""

            titleOpacity: resourceItem.hovered || layoutItemData.displayInfo ? 1.0 : 0.0
            leftContentOpacity: titleOpacity
            rightContentOpacity: resourceItem.hovered ? 1.0 : 0.0
            backgroundOpacity: Math.max(titleOpacity, leftContentOpacity, rightContentOpacity) * 0.4

            Behavior on titleOpacity { NumberAnimation { duration: 150 } }
            Behavior on rightContentOpacity { NumberAnimation { duration: 150 } }
        }
    }
}
