// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

import nx.vms.client.desktop

Item
{
    id: control

    implicitWidth: copyButton.width
    implicitHeight: copyButton.height

    /** Reference to object with text property to be copied. */
    required property var textFieldReference

    readonly property bool hovered: copyButton.hovered
    readonly property bool animationRunning: iconsCrossFade.running

    ImageButton
    {
        id: copyButton

        anchors.fill: parent

        icon.source: "image://skin/20x20/Outline/copy.svg"
        icon.color: hovered ? ColorTheme.colors.light13 : ColorTheme.colors.light16

        background: null

        onClicked: parent.copy()
    }

    ColoredImage
    {
        id: okImage

        anchors.fill: parent
        opacity: 0

        sourcePath: "image://skin/20x20/Outline/check.svg"
        sourceSize: Qt.size(20, 20)
        primaryColor: "light10"
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

            PauseAnimation { duration: 2000 }

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

            PauseAnimation { duration: 2000 }

            PropertyAnimation
            {
                target: okImage
                property: "opacity"
                to: 0
                duration: 0
            }
        }
    }

    function copy()
    {
        NxGlobals.copyToClipboard(textFieldReference.text)
        iconsCrossFade.start()
    }
}
