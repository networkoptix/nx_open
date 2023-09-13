// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.desktop

Item
{
    id: control

    width: 20
    height: 20

    readonly property bool hovered: copyButton.hovered
    readonly property bool animationRunning: iconsCrossFade.running

    ImageButton
    {
        id: copyButton

        anchors.fill: parent
        hoverEnabled: true

        icon.source: "image://svg/skin/text_buttons/copy_20.svg"
        icon.width: width
        icon.height: height
        icon.color: hovered ? ColorTheme.colors.light13 : ColorTheme.colors.light16

        background: null

        onClicked: parent.copy()
    }

    Image
    {
        id: okImage

        anchors.fill: parent
        opacity: 0

        source: "image://svg/skin/user_settings/ok.svg"
        sourceSize.width: parent.width
        sourceSize.height: parent.height
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
        NxGlobals.copyToClipboard(labelTextField.text)
        iconsCrossFade.start()
    }
}
