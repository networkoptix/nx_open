// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQml 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

ListView
{
    id: view

    property bool brief: false //< Only resource list and preview.

    property alias placeholder: controller.placeholder
    property alias tileController: controller.tileController
    property alias standardTileInteraction: controller.standardTileInteraction

    boundsBehavior: Flickable.StopAtBounds
    hoverHighlightColor: "transparent"
    bottomMargin: 8
    spacing: 1
    clip: true

    delegate: TileLoader
    {
        id: loader

        x: 8
        height: implicitHeight
        width: parent ? (parent.width - x - 16) : 0
        tileController: controller.tileController
    }

    add: Transition
    {
        id: addTransition

        NumberAnimation
        {
            property: "opacity"
            from: 0
            to: 1
            duration: 320
            easing.type: Easing.OutQuad
        }
    }

    remove: Transition
    {
        id: removeTransition

        SequentialAnimation
        {
            ScriptAction
            {
                script: removeTransition.ViewTransition.item.enabled = false
            }

            NumberAnimation
            {
                property: "opacity"
                to: 0
                duration: 320
                easing.type: Easing.OutQuad
            }
        }
    }

    displaced: Transition
    {
        id: displayTransition

        NumberAnimation
        {
            property: "y"
            duration: 320
            easing.type: Easing.OutCubic
        }

        // Finish possibly unfinished add animation.
        NumberAnimation
        {
            property: "opacity"
            duration: 320
            to: 1.0
        }
    }

    objectName: "EventRibbon"

    LoggingCategory
    {
        id: loggingCategory
        name: "Nx.RightPanel.EventRibbon"
    }

    ModelViewController
    {
        id: controller

        view: view
        loggingCategory: loggingCategory
    }

    header: Component { Column { width: parent.width }}
    footer: Component { Column { width: parent.width }}

    Rectangle
    {
        id: scrollbarPlaceholder

        parent: view.scrollBar.parent
        anchors.fill: view.scrollBar
        color: ColorTheme.colors.dark5
        visible: view.visibleArea.heightRatio >= 1.0
    }
}
