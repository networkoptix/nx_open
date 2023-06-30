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

                DeprecatedIconButton
                {
                    id: rewindBackwardButton

                    width: 32
                    height: 32
                    iconBaseName: "slider/navigation/rewind_backward_32"
                }

                DeprecatedIconButton
                {
                    id: stepBackwardButton

                    width: 32
                    height: 32
                    iconBaseName: "slider/navigation/step_backward_32"
                    visible: !timeline.playing
                }

                DeprecatedIconButton
                {
                    id: speedDownButton

                    width: 32
                    height: 32
                    iconBaseName: "slider/navigation/backward_32"
                    visible: timeline.playing
                }

                DeprecatedIconButton
                {
                    id: playPauseButton

                    width: 32
                    height: 32
                    checkable: true
                    iconBaseName: "slider/navigation/play"
                    iconCheckedBaseName: "slider/navigation/pause_32"
                }

                DeprecatedIconButton
                {
                    id: speedUpButton

                    width: 32
                    height: 32
                    iconBaseName: "slider/navigation/forward"
                    visible: timeline.playing
                }

                DeprecatedIconButton
                {
                    id: stepForwardButton

                    width: 32
                    height: 32
                    iconBaseName: "slider/navigation/step_forward"
                    visible: !timeline.playing
                }

                DeprecatedIconButton
                {
                    id: rewindForwardButton

                    width: 32
                    height: 32
                    iconBaseName: "slider/navigation/rewind_forward"
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
