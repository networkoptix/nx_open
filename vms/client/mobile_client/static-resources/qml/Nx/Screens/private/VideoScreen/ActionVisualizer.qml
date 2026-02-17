// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

import nx.client.mobile

Loader
{
    id: control

    property var action: null
    property bool externalMode: false

    readonly property Component defaultVisualizer: defaultVisualizer
    readonly property Component voiceVisualizer: voiceVisualizer

    active: !!action
    sourceComponent: action?.visualizer || defaultVisualizer
    visible: opacity > 0
    state: "hidden"

    component ActionVisualizerControl: Control
    {
        implicitHeight: 28
        horizontalPadding: 12

        background: Rectangle
        {
            opacity: externalMode ? 0.7 : 1.0
            radius: 24
            color: ColorTheme.colors.dark14
        }
    }

    Component
    {
        id: defaultVisualizer

        ActionVisualizerControl
        {
            id: control

            property string text: action.text ?? ""
            property string iconSource: action.icon ?? ""

            contentItem: Row
            {
                spacing: 6

                ColoredImage
                {
                    id: icon

                    sourcePath: control.iconSource
                    sourceSize: Qt.size(20, 20)
                    primaryColor: ColorTheme.colors.light4
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text
                {
                    id: text

                    text: control.text
                    color: ColorTheme.colors.light4
                    font.pixelSize: 14
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Component
    {
        id: voiceVisualizer

        ActionVisualizerControl
        {
            contentItem: VoiceSpectrumItem
            {
                color: ColorTheme.colors.brand
                implicitWidth: 142
                implicitHeight: 16
            }
        }
    }

    states:
    [
        State
        {
            name: "visible"
            PropertyChanges { control.opacity: 1 }
        },
        State
        {
            name: "hidden"
            PropertyChanges { control.opacity: 0 }
        }
    ]

    transitions:
    [
        Transition
        {
            from: "visible"
            to: "hidden"

            NumberAnimation
            {
                property: "opacity"
                duration: 160
                onFinished: action = null
            }
        },
        Transition
        {
            from: "hidden"
            to: "visible"

            NumberAnimation
            {
                target: control
                property: "opacity"
                duration: 80
            }
        }
    ]

    function show(visualizer, text, icon)
    {
        control.action = {
            visualizer: visualizer,
            text: text,
            icon: icon
        }
        state = "visible"
    }

    function showResult(text, icon)
    {
        hideTimer.restart()
        show(defaultVisualizer, text, icon)
    }

    function showSuccess(text)
    {
        showResult(text, "image://skin/20x20/Outline/checkmark.svg")
    }

    function showFailure(text)
    {
        showResult(text, "image://skin/20x20/Outline/terminate.svg")
    }

    function showActivity(visualizer, text)
    {
        hideTimer.stop()
        show(visualizer, text, "image://skin/20x20/Outline/loading.svg")
    }

    function showHint(text, icon, keepOpened)
    {
        if (!keepOpened)
            hideTimer.restart()
        else
            hideTimer.stop()

        show(defaultVisualizer, text, icon)
    }

    function hideDelayed()
    {
        hideTimer.restart()
    }

    function hide()
    {
        state = "hidden"
    }

    Timer
    {
        id: hideTimer

        interval: 2000
        onTriggered: control.hide()
    }
}
