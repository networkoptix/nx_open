// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

Item
{
    id: control

    implicitWidth: copyButton.width
    implicitHeight: copyButton.height

    required property string textToCopy
    readonly property bool hovered: copyButton.hovered
    readonly property bool animationRunning: iconsCrossFade.running
    property int checkShowDuration: 5000

    IconButton
    {
        id: copyButton

        anchors.fill: parent
        icon.source: "image://skin/24x24/Outline/copy.svg?primary=light4"
        icon.width: 24
        icon.height: 24

        onClicked:
        {
            NxGlobals.copyToClipboard(textToCopy)
            iconsCrossFade.start()
        }
    }

    ColoredImage
    {
        id: okImage

        anchors.fill: parent
        opacity: 0
        sourcePath: "image://skin/24x24/Outline/yes.svg?primary=light4"
        sourceSize: Qt.size(20, 20)
    }

    ParallelAnimation
    {
        id: iconsCrossFade

        SequentialAnimation
        {
            PropertyAnimation
            {
                target: copyButton
                property: "opacity"
                to: 0
                duration: 500
            }

            PauseAnimation { duration: control.checkShowDuration }

            PropertyAnimation
            {
                target: copyButton
                property: "opacity"
                to: 1
                duration: 0
            }
        }

        SequentialAnimation
        {
            PropertyAnimation
            {
                target: okImage
                property: "opacity"
                to: 1
                duration: 500
            }

            PauseAnimation { duration: control.checkShowDuration }

            PropertyAnimation
            {
                target: okImage
                property: "opacity"
                to: 0
                duration: 0
            }
        }
    }
}
