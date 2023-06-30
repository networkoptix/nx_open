// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Core 1.0

Control
{
    id: sidePanel

    property int edge: Qt.LeftEdge
    property bool shown: true //< Visibility via opacity.
    property bool opened: false //< Fully or partially slid in.
    readonly property bool closed: !opened && !positionAnimation.running //< Fully slid out.

    property real borderWidth: 1
    property alias borderColor: border.color
    property real shadowWidth: 1
    property alias shadowColor: shadow.color

    property color backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.85)

    property bool openCloseButtonVisible: true

    property real openCloseDuration: 300
    property real openDuration: openCloseDuration
    property real closeDuration: openCloseDuration

    property real fadeDuration: 200
    property real fadeInDuration: fadeDuration
    property real fadeOutDuration: fadeDuration

    anchors.left: (parent && edge !== Qt.RightEdge) ? parent.left : undefined
    anchors.right: (parent && edge !== Qt.LeftEdge) ? parent.right : undefined
    anchors.top: (parent && edge !== Qt.BottomEdge) ? parent.top : undefined
    anchors.bottom: (parent && edge !== Qt.TopEdge) ? parent.bottom : undefined

    // Private.

    opacity: animatedOpacity
    visible: opacity > 0.0
    enabled: visible

    background: Rectangle { color: backgroundColor }

    onContentItemChanged:
    {
        if (contentItem)
            contentItem.visible = Qt.binding(function() { return !sidePanel.closed })
    }

    Rectangle
    {
        id: border

        color: ColorTheme.colors.dark8

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    DeprecatedIconButton
    {
        id: button

        width: 12
        height: 32
        iconDir: "qrc:/skin/panel"
        iconBaseName: sidePanel.opened ? "slide_left" : "slide_right"
        enabled: openCloseButtonVisible
        opacity: openCloseButtonVisible ? 1.0 : 0.0
        visible: opacity > 0.0

        onClicked: sidePanel.opened = !sidePanel.opened

        Behavior on opacity
        {
            NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
        }
    }

    Rectangle
    {
        id: shadow

        color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.15)

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    states:
    [
        State
        {
            name: "left"
            when: sidePanel.edge === Qt.LeftEdge

            AnchorChanges
            {
                target: sidePanel
                anchors.left: parent ? parent.left : undefined
            }

            AnchorChanges { target: border; anchors.left: undefined }
            AnchorChanges { target: shadow; anchors.left: parent.right; anchors.right: undefined }

            AnchorChanges
            {
                target: button
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.right
            }

            PropertyChanges
            {
                target: sidePanel
                anchors.leftMargin: animatedPositionMargin
                restoreEntryValues: true
            }

            PropertyChanges { target: border; width: sidePanel.borderWidth }
            PropertyChanges { target: shadow; width: sidePanel.shadowWidth }

            PropertyChanges
            {
                target: button
                anchors.horizontalCenterOffset: width / 2
                anchors.verticalCenterOffset: 0
                rotation: 0
            }
        },

        State
        {
            name: "right"
            when: sidePanel.edge === Qt.RightEdge

            AnchorChanges
            {
                target: sidePanel
                anchors.right: parent ? parent.right : undefined
            }

            AnchorChanges { target: border; anchors.right: undefined }
            AnchorChanges { target: shadow; anchors.left: undefined; anchors.right: parent.left }

            AnchorChanges
            {
                target: button
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.left
            }

            PropertyChanges
            {
                target: sidePanel
                anchors.rightMargin: animatedPositionMargin
                restoreEntryValues: true
            }

            PropertyChanges { target: border; width: sidePanel.borderWidth }
            PropertyChanges { target: shadow; width: sidePanel.shadowWidth }

            PropertyChanges
            {
                target: button
                anchors.horizontalCenterOffset: -width / 2
                anchors.verticalCenterOffset: 0
                rotation: 180
            }
        },

        State
        {
            name: "top"
            when: sidePanel.edge === Qt.TopEdge

            AnchorChanges
            {
                target: sidePanel
                anchors.top: parent ? parent.top : undefined
            }

            AnchorChanges { target: border; anchors.top: undefined }
            AnchorChanges { target: shadow; anchors.top: parent.bottom; anchors.bottom: undefined }

            AnchorChanges
            {
                target: button
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.bottom
            }

            PropertyChanges
            {
                target: sidePanel
                anchors.topMargin: animatedPositionMargin
                restoreEntryValues: true
            }

            PropertyChanges { target: border; height: sidePanel.borderWidth }
            PropertyChanges { target: shadow; height: sidePanel.shadowWidth }

            PropertyChanges
            {
                target: button
                anchors.horizontalCenterOffset: 0
                anchors.verticalCenterOffset: width / 2
                rotation: 90
            }
        },

        State
        {
            name: "bottom"
            when: sidePanel.edge === Qt.BottomEdge

            AnchorChanges
            {
                target: sidePanel
                anchors.bottom: parent ? parent.bottom : undefined
            }

            AnchorChanges { target: border; anchors.bottom: undefined }
            AnchorChanges { target: shadow; anchors.top: undefined; anchors.bottom: parent.top }

            AnchorChanges
            {
                target: button
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.top
            }

            PropertyChanges
            {
                target: sidePanel
                anchors.bottomMargin: animatedPositionMargin
                restoreEntryValues: true
            }

            PropertyChanges { target: border; height: sidePanel.borderWidth }
            PropertyChanges { target: shadow; height: sidePanel.shadowWidth }

            PropertyChanges
            {
                target: button
                anchors.horizontalCenterOffset: 0
                anchors.verticalCenterOffset: -width / 2
                rotation: 270
            }
        }
    ]

    Behavior on animatedOpacity
    {
        NumberAnimation
        {
            id: opacityAnimation
            easing.type: sidePanel.shown ? Easing.InOutQuad : Easing.OutQuad
            duration: sidePanel.shown ? sidePanel.fadeInDuration : sidePanel.fadeOutDuration
        }
    }

    Behavior on animatedPositionMargin
    {
        SmoothedAnimation
        {
            id: positionAnimation
            easing.type: sidePanel.opened ? Easing.InOutQuad : Easing.OutQuad
            duration: sidePanel.opened ? sidePanel.openDuration : sidePanel.closeDuration
        }
    }

    property real animatedOpacity: shown ? 1.0 : 0.0

    property real animatedPositionMargin:
    {
        switch (edge)
        {
            case Qt.LeftEdge:
            case Qt.RightEdge:
                return (opened && visible) ? 0 : -width

            case Qt.TopEdge:
            case Qt.BottomEdge:
                return (opened && visible) ? 0 : -height

            return 0
        }
    }
}
