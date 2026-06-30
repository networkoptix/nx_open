// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

Item
{
    property T.ScrollView view: parent
    property Flickable flickable: view.contentItem

    z: 1

    component ShadowItem: Rectangle
    {
        implicitWidth: parent?.width ?? 100
        implicitHeight: 24

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: "#0D0F12" }
            GradientStop { position: 1.0; color: "transparent" }
        }

        Behavior on opacity { NumberAnimation { duration: 100 } }
    }

    ShadowItem
    {
        id: topShadow

        anchors.top: parent.top
        height: 24
        opacity: flickable.atYBeginning ? 0.0 : 1.0
        rotation: 0
    }

    ShadowItem
    {
        id: bottomShadow

        anchors.bottom: parent.bottom
        height: 54
        opacity: flickable.atYEnd ? 0.0 : 1.0
        rotation: 180
    }
}
