// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Controls 1.0

Item
{
    id: timeline
    property alias playing: playPauseButton.checked

    implicitHeight: 88

    Item
    {
        id: navigationArea
        width: 176

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        component NavigationButton: ImageButton
        {
            id: navigationButton

            icon.width: 32
            icon.height: 32

            primaryColor: (down || checked) ? "light2" : (hovered ? "light6" : "light4")
            secondaryColor: (down || checked) ? "dark6" : (hovered ? "dark8" : "dark7")

            background: null
        }

        Column
        {
            leftPadding: 6

            Item
            {
                id: speedSlider

                // TODO: implement me.

                width: 10
                height: 28
            }

            Row
            {
                spacing: 1

                NavigationButton
                {
                    id: rewindBackwardButton

                    icon.source: "image://skin/slider/navigation/rewind_backward_32.svg"
                }

                NavigationButton
                {
                    id: stepBackwardButton

                    icon.source: "image://skin/slider/navigation/step_backward_32.svg"
                    visible: !timeline.playing
                }

                NavigationButton
                {
                    id: speedDownButton

                    icon.source: "image://skin/slider/navigation/backward_32.svg"
                    visible: timeline.playing
                }

                NavigationButton
                {
                    id: playPauseButton

                    checkable: true

                    icon.source: checked
                        ? "image://skin/slider/navigation/pause_32.svg"
                        : "image://skin/slider/navigation/play_32.svg"
                }

                NavigationButton
                {
                    id: speedUpButton

                    icon.source: "image://skin/slider/navigation/forward_32.svg"
                    visible: timeline.playing
                }

                NavigationButton
                {
                    id: stepForwardButton

                    icon.source: "image://skin/slider/navigation/step_forward_32.svg"
                    visible: !timeline.playing
                }

                NavigationButton
                {
                    id: rewindForwardButton

                    icon.source: "image://skin/slider/navigation/rewind_forward_32.svg"
                }
            }

            Text
            {
                id: clockLabel

                height: 28
                color: ColorTheme.colors.brand_d2
                font.pixelSize: 18
                font.weight: Font.Medium
                verticalAlignment: Text.AlignVCenter

                Timer
                {
                    interval: 1000
                    repeat: true
                    triggeredOnStart: true
                    running: true

                    onTriggered:
                    {
                        // TODO: #vkutin Use application-wide time management functions
                        // to show system time formatted in accordance with our settings.
                        clockLabel.text = new Date().toLocaleTimeString(Qt.locale(), "H:mm:ss");
                    }
                }
            }
        }
    }

    Rectangle
    {
        id: mainArea
        color: ColorTheme.colors.dark6

        anchors.top: parent.top
        anchors.left: navigationArea.right
        anchors.right: controlsArea.left
        anchors.bottom: parent.bottom
    }

    Item
    {
        id: controlsArea
        width: 117

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }
}
