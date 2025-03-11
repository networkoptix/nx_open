// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

Popup
{
    id: breadcrumbPopup

    height: 72

    property alias model: breadcrumbList.model

    signal itemClicked(var nodeId)

    padding: 0
    popupType: Popup.Item

    onAboutToShow:
    {
        breadcrumbList.positionViewAtEnd()
        leftFadeBehavior.enabled = false
        rightFadeBehavior.enabled = false
        leftFade.opacity = 1
        rightFade.opacity = 0
        fadeTimer.start()
    }

    background: Rectangle
    {
        width: breadcrumbPopup.width
        height: breadcrumbPopup.height
        color: ColorTheme.colors.dark10
    }

    ListView
    {
        id: breadcrumbList

        orientation: ListView.Horizontal
        anchors.centerIn: parent
        height: parent.height
        width: Math.min(contentWidth, parent.width)
        boundsBehavior: width < parent.width
            ? Flickable.StopAtBounds
            : Flickable.DragAndOvershootBounds
        spacing: 0
        clip: false

        header: Item { width: 20; height: 1 }
        footer: Item { width: 20; height: 1 }

        delegate: Item
        {
            readonly property bool lastItem: index == breadcrumbList.model.length - 1
            width: breadcrumbButtonBackground.width + (lastItem ? 0 : (20 + 4 * 2))
            height: parent.height

            Rectangle
            {
                id: breadcrumbButtonBackground

                width: breadcrumbButtonText.implicitWidth + 24
                height: 32
                anchors.verticalCenter: parent.verticalCenter

                color: ColorTheme.colors.dark14
                radius: 8

                Text
                {
                    id: breadcrumbButtonText
                    anchors.centerIn: parent
                    text: modelData.name
                    color: ColorTheme.colors.light4
                    font.pixelSize: 14
                    font.weight: Font.Normal
                }

                TapHandler
                {
                    gesturePolicy: TapHandler.WithinBounds
                    onSingleTapped:
                    {
                        breadcrumbPopup.itemClicked(modelData.nodeId)
                        breadcrumbPopup.close()
                    }
                }
            }

            ColoredImage
            {
                width: 20
                height: 20
                visible: !lastItem

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 4
                sourceSize: Qt.size(20, 20)
                sourcePath: "image://skin/20x20/Outline/arrow_right.svg?primary=light10"
            }
        }
    }

    Timer
    {
        id: fadeTimer

        interval: 100
        running: breadcrumbPopup.opened
        repeat: false
        onTriggered:
        {
            leftFade.opacity = Qt.binding(() => breadcrumbList.atXBeginning ? 0 : 1)
            rightFade.opacity = Qt.binding(() => breadcrumbList.atXEnd ? 0 : 1)
            leftFadeBehavior.enabled = true
            rightFadeBehavior.enabled = true
        }
    }

    Rectangle
    {
        id: leftFade

        opacity: 1
        width: 45
        height: parent.height
        gradient: Gradient
        {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.15, 0.19, 0.21, 1) }
            GradientStop { position: 1.0; color: Qt.rgba(0.15, 0.19, 0.21, 0) }
        }

        Behavior on opacity
        {
            id: leftFadeBehavior
            NumberAnimation
            { 
                duration: 100
                easing.type: Easing.InOutQuad
            }
        }
    }

    Rectangle
    {
        id: rightFade

        opacity: 0
        width: 45
        height: parent.height
        anchors.right: parent.right
        gradient: Gradient
        {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.15, 0.19, 0.21, 0) }
            GradientStop { position: 1.0; color: Qt.rgba(0.15, 0.19, 0.21, 1) }
        }

        Behavior on opacity
        {
            id: rightFadeBehavior
            NumberAnimation
            { 
                duration: 100
                easing.type: Easing.InOutQuad
            }
        }
    }
}
